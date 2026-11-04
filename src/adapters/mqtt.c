/* SPDX-License-Identifier: MIT */
/*
 * MQTT adapter: one connection to the Broker over plain TCP, using Zephyr's
 * MQTT client (MQTT 3.1.1, QoS 0).
 *
 * The adapter's own thread owns the connection: it resolves the host,
 * connects, keeps the connection alive and reports CONNECTED or DISCONNECTED
 * to the core. The core's connect and disconnect requests only hand that
 * thread new work, so they never block the core. Every request starts a new
 * session; events from an older session are not reported.
 *
 * Messages arrive only on the command topic the core subscribed to; the
 * thread reads each one and reports it as NODE_EVENT_COMMAND.
 */
#include "adapters.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/net/mqtt.h>
#include <zephyr/net/socket.h>
#include <zephyr/sys/atomic.h>

#include "../app.h"

LOG_MODULE_REGISTER(mqtt_adapter, LOG_LEVEL_INF);

#define MQTT_BUF_SIZE   1024
#define POLL_PERIOD_MS  250
#define THREAD_STACK    4096
#define THREAD_PRIORITY 7
#define TOPIC_MAX       128
#define WILL_MAX        16

/* The latest request from the core. Guarded by request_lock. */
struct request {
	uint32_t session;
	bool connect;
	char host[NODE_BROKER_HOST_MAX_LEN + 1];
	uint16_t port;
	char client_id[NODE_ID_LEN + 1];
	bool has_username;
	char username[NODE_BROKER_USERNAME_MAX_LEN + 1];
	bool has_password;
	char password[NODE_BROKER_PASSWORD_MAX_LEN + 1];
	char will_topic[TOPIC_MAX];
	char will_payload[WILL_MAX];
	bool will_retained;
};

static struct request request;
static K_MUTEX_DEFINE(request_lock);
static K_SEM_DEFINE(request_sem, 0, 1);

/* The session the thread is running, a copy of the request that started it. */
static struct request current;

/* The client and its buffers. Guarded by client_lock. */
static struct mqtt_client client;
static uint8_t rx_buf[MQTT_BUF_SIZE];
static uint8_t tx_buf[MQTT_BUF_SIZE];
static struct net_sockaddr_storage broker_addr;
static struct mqtt_utf8 username;
static struct mqtt_utf8 password;
static struct mqtt_topic will_topic;
static struct mqtt_utf8 will_message;
static K_MUTEX_DEFINE(client_lock);

/* The topic the core subscribed to. Guarded by client_lock. */
static char command_topic[TOPIC_MAX];

/*
 * Connected to the Broker: publishes may go out. It stays set until the
 * thread notices a new request, so a publish made just before the core's
 * disconnect request (its "offline") still reaches the Broker.
 */
static atomic_t connected;

/* Set by the client's event callback, read by the thread. */
static bool connack_ok;
static bool connection_closed;

static bool is_current(uint32_t session)
{
	k_mutex_lock(&request_lock, K_FOREVER);
	bool current_session = request.session == session;

	k_mutex_unlock(&request_lock);
	return current_session;
}

/* Report to the core, unless the core has moved on to a newer session. */
static void report(uint32_t session, enum node_event_type type)
{
	if (is_current(session)) {
		app_post_event(&(struct node_event){.type = type});
	}
}

static bool is_command_topic(const struct mqtt_utf8 *topic)
{
	return topic->size == strlen(command_topic) &&
	       memcmp(topic->utf8, command_topic, topic->size) == 0;
}

/*
 * A message arrived: read all of its payload, as the client requires, and
 * report it as a command. Runs in mqtt_input(), holding client_lock.
 */
static void on_publish(struct mqtt_client *c, const struct mqtt_publish_param *pub)
{
	struct node_event event = {.type = NODE_EVENT_COMMAND};
	const struct mqtt_utf8 *topic = &pub->message.topic.topic;
	/* Check the topic first: it lives in rx_buf, read before the payload. */
	bool is_command = is_command_topic(topic);
	uint32_t len = pub->message.payload.len;
	size_t keep = MIN(len, sizeof(event.command.payload));
	uint8_t discard[32];

	if (!is_command) {
		LOG_WRN("ignored a message on %.*s", (int)topic->size, (const char *)topic->utf8);
	}
	int err = mqtt_readall_publish_payload(c, (uint8_t *)event.command.payload, keep);

	for (uint32_t left = len - keep; err == 0 && left > 0;) {
		size_t n = MIN(left, sizeof(discard));

		err = mqtt_readall_publish_payload(c, discard, n);
		left -= n;
	}
	if (err) {
		LOG_WRN("reading a message failed: %d", err);
		return;
	}
	if (!is_command || !is_current(current.session)) {
		return;
	}
	event.command.len = keep;
	event.command.truncated = len > keep;
	event.command.retained = pub->retain_flag;
	app_post_event(&event);
}

static void mqtt_event_handler(struct mqtt_client *c, const struct mqtt_evt *evt)
{
	switch (evt->type) {
	case MQTT_EVT_CONNACK:
		if (evt->result == 0) {
			connack_ok = true;
		} else {
			LOG_WRN("Broker refused the connection: %d", evt->result);
			connection_closed = true;
		}
		break;
	case MQTT_EVT_DISCONNECT:
		connection_closed = true;
		break;
	case MQTT_EVT_PUBLISH:
		if (evt->result == 0) {
			on_publish(c, &evt->param.publish);
		}
		break;
	case MQTT_EVT_SUBACK:
		LOG_INF("subscribed to commands");
		break;
	default:
		break;
	}
}

static int resolve(const char *host, uint16_t port)
{
	struct zsock_addrinfo hints = {
		.ai_family = NET_AF_INET,
		.ai_socktype = NET_SOCK_STREAM,
	};
	struct zsock_addrinfo *res;
	char service[6];

	snprintf(service, sizeof(service), "%u", port);
	int err = zsock_getaddrinfo(host, service, &hints, &res);

	if (err) {
		LOG_WRN("can't resolve Broker \"%s\": %d (errno %d)", host, err,
			err == DNS_EAI_SYSTEM ? errno : 0);
		return -EHOSTUNREACH;
	}
	memcpy(&broker_addr, res->ai_addr, MIN(res->ai_addrlen, sizeof(broker_addr)));
	zsock_freeaddrinfo(res);
	return 0;
}

static struct mqtt_utf8 utf8(const char *s)
{
	return (struct mqtt_utf8){.utf8 = (const uint8_t *)s, .size = strlen(s)};
}

/* Set up the client for `current` and send CONNECT. Holds client_lock. */
static int start_connect(void)
{
	mqtt_client_init(&client);
	client.broker = &broker_addr;
	client.evt_cb = mqtt_event_handler;
	client.client_id = utf8(current.client_id);
	client.protocol_version = MQTT_VERSION_3_1_1;
	client.transport.type = MQTT_TRANSPORT_NON_SECURE;
	client.rx_buf = rx_buf;
	client.rx_buf_size = sizeof(rx_buf);
	client.tx_buf = tx_buf;
	client.tx_buf_size = sizeof(tx_buf);
	client.clean_session = 1;

	if (current.has_username) {
		username = utf8(current.username);
		client.user_name = &username;
	}
	if (current.has_password) {
		password = utf8(current.password);
		client.password = &password;
	}
	will_topic = (struct mqtt_topic){
		.topic = utf8(current.will_topic),
		.qos = MQTT_QOS_0_AT_MOST_ONCE,
	};
	will_message = utf8(current.will_payload);
	client.will_topic = &will_topic;
	client.will_message = &will_message;
	client.will_retain = current.will_retained;

	connack_ok = false;
	connection_closed = false;
	return mqtt_connect(&client);
}

/* Keep the connection going until it closes or the core asks for another. */
static void run_connection(uint32_t session)
{
	struct zsock_pollfd fd = {.events = ZSOCK_POLLIN};

	while (is_current(session)) {
		k_mutex_lock(&client_lock, K_FOREVER);
		fd.fd = client.transport.tcp.sock;
		int keepalive_ms = mqtt_keepalive_time_left(&client);
		int timeout = keepalive_ms >= 0 ? MIN(POLL_PERIOD_MS, keepalive_ms)
						: POLL_PERIOD_MS;
		k_mutex_unlock(&client_lock);

		int ready = zsock_poll(&fd, 1, timeout);

		k_mutex_lock(&client_lock, K_FOREVER);
		int err = 0;

		if (ready > 0) {
			err = mqtt_input(&client);
		}
		if (err == 0 && !connection_closed) {
			err = mqtt_live(&client);
			err = err == -EAGAIN ? 0 : err;
		}
		if (err && !connection_closed) {
			LOG_WRN("Broker connection failed: %d", err);
			connection_closed = true;
		}
		if (connection_closed) {
			/* Frees the socket; does nothing if the client already did. */
			mqtt_abort(&client);
		}
		bool now_connected = connack_ok && !connection_closed;
		bool closed = connection_closed;
		k_mutex_unlock(&client_lock);

		if (now_connected && !atomic_get(&connected)) {
			atomic_set(&connected, 1);
			LOG_INF("connected to the Broker");
			report(session, NODE_EVENT_BROKER_CONNECTED);
		}
		if (closed) {
			atomic_set(&connected, 0);
			LOG_INF("disconnected from the Broker");
			report(session, NODE_EVENT_BROKER_DISCONNECTED);
			return;
		}
	}

	/* The core asked for a disconnect or a new connection. */
	atomic_set(&connected, 0);
	k_mutex_lock(&client_lock, K_FOREVER);
	if (mqtt_disconnect(&client, NULL) != 0) {
		mqtt_abort(&client);
	}
	k_mutex_unlock(&client_lock);
}

static void run_session(void)
{
	k_mutex_lock(&request_lock, K_FOREVER);
	current = request;
	k_mutex_unlock(&request_lock);

	if (!current.connect) {
		return;
	}

	LOG_INF("connecting to Broker %s:%u", current.host, current.port);
	int err = resolve(current.host, current.port);

	if (err == 0) {
		k_mutex_lock(&client_lock, K_FOREVER);
		err = start_connect();
		k_mutex_unlock(&client_lock);
	}
	if (err) {
		LOG_WRN("connecting to the Broker failed: %d", err);
		report(current.session, NODE_EVENT_BROKER_DISCONNECTED);
		return;
	}
	run_connection(current.session);
}

static void mqtt_thread(void *p1, void *p2, void *p3)
{
	ARG_UNUSED(p1);
	ARG_UNUSED(p2);
	ARG_UNUSED(p3);

	while (true) {
		k_sem_take(&request_sem, K_FOREVER);
		run_session();
	}
}

K_THREAD_DEFINE(mqtt_adapter_thread, THREAD_STACK, mqtt_thread, NULL, NULL, NULL,
		THREAD_PRIORITY, 0, 0);

static void copy(char *dst, size_t size, const char *src)
{
	snprintf(dst, size, "%s", src != NULL ? src : "");
}

int mqtt_adapter_connect(void *ctx, const struct node_mqtt_connect *params)
{
	ARG_UNUSED(ctx);

	k_mutex_lock(&request_lock, K_FOREVER);
	request.session++;
	request.connect = true;
	copy(request.host, sizeof(request.host), params->host);
	request.port = params->port;
	copy(request.client_id, sizeof(request.client_id), params->client_id);
	request.has_username = params->username != NULL;
	copy(request.username, sizeof(request.username), params->username);
	request.has_password = params->password != NULL;
	copy(request.password, sizeof(request.password), params->password);
	copy(request.will_topic, sizeof(request.will_topic), params->will_topic);
	copy(request.will_payload, sizeof(request.will_payload), params->will_payload);
	request.will_retained = params->will_retained;
	k_mutex_unlock(&request_lock);

	k_sem_give(&request_sem);
	return 0;
}

int mqtt_adapter_disconnect(void *ctx)
{
	ARG_UNUSED(ctx);

	k_mutex_lock(&request_lock, K_FOREVER);
	request.session++;
	request.connect = false;
	k_mutex_unlock(&request_lock);

	k_sem_give(&request_sem);
	return 0;
}

int mqtt_adapter_publish(void *ctx, const char *topic, const void *payload, size_t len,
			 bool retained)
{
	ARG_UNUSED(ctx);

	if (!atomic_get(&connected)) {
		return -ENOTCONN;
	}

	struct mqtt_publish_param param = {
		.message.topic = {.topic = utf8(topic), .qos = MQTT_QOS_0_AT_MOST_ONCE},
		.message.payload = {.data = (uint8_t *)payload, .len = len},
		.retain_flag = retained,
	};

	k_mutex_lock(&client_lock, K_FOREVER);
	int err = mqtt_publish(&client, &param);
	k_mutex_unlock(&client_lock);

	if (err) {
		LOG_WRN("publish to %s failed: %d", topic, err);
	}
	return err;
}

int mqtt_adapter_subscribe(void *ctx, const char *topic)
{
	ARG_UNUSED(ctx);
	static uint16_t message_id;

	if (!atomic_get(&connected)) {
		return -ENOTCONN;
	}

	k_mutex_lock(&client_lock, K_FOREVER);
	copy(command_topic, sizeof(command_topic), topic);

	const struct mqtt_topic subscription = {
		.topic = utf8(command_topic),
		.qos = MQTT_QOS_0_AT_MOST_ONCE,
	};
	/* Message IDs must not be 0. */
	message_id = message_id == UINT16_MAX ? 1 : message_id + 1;
	const struct mqtt_subscription_list list = {
		.list = &subscription,
		.list_count = 1,
		.message_id = message_id,
	};
	int err = mqtt_subscribe(&client, &list);

	k_mutex_unlock(&client_lock);

	if (err) {
		LOG_WRN("subscribing to %s failed: %d", topic, err);
	}
	return err;
}
