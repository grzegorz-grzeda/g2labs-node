# Managing a Node over the network

Once a Node has an IP address on a Known Network, it answers SMP (MCUmgr's Simple Management Protocol) on **UDP port 1337**, on every interface. SMP has four groups here:

| Group | What it does |
|---|---|
| Image | Upload an image to the secondary slot, list both slots, mark an image for test or confirm it |
| OS | Echo, reset, and the buffer size and bootloader info that host tools size uploads from |
| Shell | Run any UART shell command (`node …` and the rest) and get its output |
| Settings | Read and change the Node's settings, without their passwords |

The Node's IP address is in `node network status` on the UART shell (or `scripts/g2labs-node.py show`).

> **No authentication.** Anyone who can reach a Node's UDP port 1337 can upload an image, run shell commands, change its settings or reset it. This is a known gap until the security work (#1, Out of Scope). Keep Nodes on a network you trust.

## Host tools

[`smpmgr`](https://github.com/intercreate/smpmgr) (Python, `pip install smpmgr`) does images, OS and the shell. This repo adds a settings group to it as a plugin, `scripts/smpmgr/settings_group.py`, loaded with `--plugin-path=scripts/smpmgr` (it needs `=`). Every command takes `--ip <node-ip>`.

The Go [`mcumgr`](https://github.com/apache/mynewt-mcumgr-cli) CLI also works for images, reset and the shell, with `--conntype udp --connstring "[<node-ip>]:1337"`. It has no settings commands.

## Images

Build the image as usual. The file to upload is the signed one, `build-c6/g2labs-node/zephyr/zephyr.signed.bin` (`build-s3/…` for the S3).

1. Upload it to the secondary slot:

   ```bash
   smpmgr --ip 192.168.1.50 image upload build-c6/g2labs-node/zephyr/zephyr.signed.bin
   ```

2. List both slots, which gives you the new image's hash:

   ```bash
   smpmgr --ip 192.168.1.50 image state-read
   ```

3. Mark it for test. MCUboot swaps it in on the next reset:

   ```bash
   smpmgr --ip 192.168.1.50 image state-write <hash>
   ```

4. Reset the Node:

   ```bash
   smpmgr --ip 192.168.1.50 os reset
   ```

The new image boots as a **test image**. It confirms itself on its first successful Broker connection; until then MCUboot reverts to the previous image on the next reboot. So an image that can't reach the Broker (it breaks Wi-Fi, MQTT, or a setting it reads) is undone by one more reset, from SMP if the Node is still reachable there (`smpmgr --ip … os reset`), else by power-cycling it. An image that hangs is reset by the hardware watchdog, which the main loop feeds: if the loop stalls for `CONFIG_NODE_WATCHDOG_TIMEOUT_S` (30 s by default; the SoC may allow up to twice that), the Node resets, and an image not yet confirmed reverts.

To keep a test image that can't reach a Broker (on a bench without one, say), confirm it yourself with `smpmgr --ip … image state-write --confirm`, which confirms the image that is running.

On the UART shell, `node image list` (`scripts/g2labs-node.py image list`) shows the same slots and states:

```
Slot 0: 0.2.0 active confirmed
  hash 3f1c…
Slot 1: 0.3.0 pending
  hash 9a07…
```

| State | Meaning |
|---|---|
| active | the image running now |
| confirmed | MCUboot keeps it; it won't be reverted |
| pending | MCUboot swaps to it on the next reboot, as a test |
| permanent | pending, and confirmed once swapped in |

## Shell

Any shell command runs over SMP, and its output comes back:

```bash
smpmgr --ip 192.168.1.50 shell "node broker show"
smpmgr --ip 192.168.1.50 shell "node network status"
```

The `node …` commands hide passwords here too.

## Settings

| Key | Value |
|---|---|
| `node/name` | the friendly name |
| `node/broker/host`, `node/broker/port` | the Broker; an empty port means 1883 |
| `node/broker/username`, `node/broker/password` | the Broker login |
| `node/kn/<i>/ssid`, `node/kn/<i>/password` | Known Network `<i>`, from 0 to 4, in the order `node network list` shows (from 1 there) |

Values are text. Reads return what the running Node uses. **Passwords are never returned**: reading any `…/password` key is refused.

Writes are **staged**. A **commit** applies all of them to the Node together, and saves what the Node then holds. You never need a separate save. So a Known Network's SSID and password, or the Broker's host and port, change together. Only the keys you wrote change. A change made on the UART shell between your writes and the commit is kept.

```bash
S="smpmgr --plugin-path=scripts/smpmgr --ip 192.168.1.50"
$S settings read node/broker/host
$S settings write node/broker/host hub.lan
$S settings write node/broker/port 1883
$S settings commit
```

- An empty value clears a key: `settings write node/name ""` clears the friendly name, and an empty `node/kn/<i>/ssid` removes that Known Network.
- A new SSID needs its password written in the same commit (empty for an open network). Otherwise the Node would join the new network with the old network's password, so it leaves that Known Network unchanged. A new Known Network goes to the end of the list, and the indexes close up after a removal, so read the SSIDs back after a commit.
- A new Broker applies at once, and the Node reconnects. A change of Known Networks leaves the Node on its current network while that network is healthy, as on the UART shell.
- The Node checks committed values the way the shell does: password lengths, a password needs a username, and so on. A friendly name or Broker that fails the check is not applied. A Known Network that fails it is not applied either, and the network it would have replaced stays. The Node logs why, but the commit still answers success (Zephyr ignores the result), so read the keys back to see what was applied.
- Deleting a `node/…` key is refused: write an empty value instead. Load is refused too, because settings are loaded at boot and saved on every commit.
