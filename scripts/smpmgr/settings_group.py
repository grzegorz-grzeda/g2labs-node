# SPDX-License-Identifier: MIT
"""smpmgr plugin: the SMP settings management group, which smpmgr lacks.

    smpmgr --plugin-path=scripts/smpmgr --ip <node-ip> settings read node/broker/host
    smpmgr --plugin-path=scripts/smpmgr --ip <node-ip> settings write node/broker/host hub.lan
    smpmgr --plugin-path=scripts/smpmgr --ip <node-ip> settings commit

A Node stages writes until a commit, then applies and saves them. See
docs/management.md for the keys.
"""

import asyncio
from typing import cast

import typer
from rich import print
from smpclient.generics import error, success
from smpclient.requests.settings_management import CommitSettings, ReadSetting, WriteSetting

from smpmgr.common import Options, connect_with_spinner, get_smpclient, smp_request

app = typer.Typer(name="settings", help="The SMP Settings Management Group.")


def _run(ctx: typer.Context, request, describe) -> None:
    options = cast(Options, ctx.obj)
    smpclient = get_smpclient(options)

    async def f() -> None:
        await connect_with_spinner(smpclient, options.timeout)
        response = await smp_request(smpclient, options, request)
        if error(response):
            print(response)
            raise typer.Exit(code=1)
        if success(response):
            describe(response)

    asyncio.run(f())


@app.command()
def read(ctx: typer.Context, name: str) -> None:
    """Read a setting; prints its value as text."""

    _run(ctx, ReadSetting(name=name), lambda r: print(r.val.decode(errors="replace")))


@app.command()
def write(ctx: typer.Context, name: str, value: str) -> None:
    """Stage a setting as text; an empty value clears it. Applied on commit."""

    _run(ctx, WriteSetting(name=name, val=value.encode()), lambda r: None)


@app.command()
def commit(ctx: typer.Context) -> None:
    """Apply and save the staged settings."""

    _run(ctx, CommitSettings(), lambda r: print("Committed"))
