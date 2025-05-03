#!/usr/bin/env python3
"""
load_test.py – simple chat‑server load generator

Usage example
-------------
python3 load_test.py --host 127.0.0.1 --port 2000 \
                     --clients 100 --rate 2 --duration 30

That command spawns 100 concurrent TCP clients.
Each client sends 2 messages per second for 30 s, then the
script prints aggregated throughput/latency stats.
"""

import argparse
import asyncio
import random
import statistics
import string
import time


def random_msg(size: int = 64) -> str:
    """Return a random ASCII string of *size* characters."""
    return "".join(random.choice(string.ascii_letters) for _ in range(size))


class Client:
    """A single asynchronous chat client used for load‑testing."""

    def __init__(self, host: str, port: int, rate: float):
        self.host = host
        self.port = port
        self.rate = rate
        self.latencies_ms: list[float] = []

    async def run(self) -> None:
        reader, writer = await asyncio.open_connection(self.host, self.port)
        try:
            while True:
                payload = random_msg().encode() + b"\n"          # newline‑terminated frame
                t0 = time.perf_counter_ns()

                writer.write(payload)
                await writer.drain()

                await reader.readline()                          # wait for echo/broadcast
                t1 = time.perf_counter_ns()
                self.latencies_ms.append((t1 - t0) / 1_000_000)  # convert ns → ms

                await asyncio.sleep(1 / self.rate)
        except asyncio.CancelledError:
            writer.close()
            await writer.wait_closed()


async def main(args: argparse.Namespace) -> None:
    clients = [Client(args.host, args.port, args.rate) for _ in range(args.clients)]
    tasks = [asyncio.create_task(c.run()) for c in clients]

    # Run the workload for *duration* seconds
    await asyncio.sleep(args.duration)

    # Cancel the tasks and gather results
    for t in tasks:
        t.cancel()
    await asyncio.gather(*tasks, return_exceptions=True)

    all_lat = [l for c in clients for l in c.latencies_ms]
    if not all_lat:
        print("No messages acknowledged – server unreachable?")
        return

    print(f"\nMessages sent: {len(all_lat)}")
    print(f"Mean latency:  {statistics.mean(all_lat):.2f} ms")
    print(f"Median:        {statistics.median(all_lat):.2f} ms")
    print(f"p95 latency:   {statistics.quantiles(all_lat, n=100)[94]:.2f} ms")
    print(f"p99 latency:   {statistics.quantiles(all_lat, n=100)[98]:.2f} ms")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Async chat‑server load generator")
    parser.add_argument("--host", default="127.0.0.1",
                        help="Server hostname/IP (default: 127.0.0.1)")
    parser.add_argument("--port", type=int, default=2000,
                        help="Server TCP port (default: 2000)")
    parser.add_argument("--clients", type=int, default=10,
                        help="Number of concurrent clients (default: 10)")
    parser.add_argument("--rate", type=float, default=1.0,
                        help="Messages per second, per client (default: 1.0)")
    parser.add_argument("--duration", type=int, default=30,
                        help="Test duration in seconds (default: 30)")
    asyncio.run(main(parser.parse_args()))
