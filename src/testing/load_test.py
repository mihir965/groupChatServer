#!/usr/bin/env python3
"""
load_test.py - load generator

Refernces - utilized chatGPT selectively to generate the testing and evaluation code in this file.

example -

python3 load_test.py --host 127.0.0.1 --port 2000  --clients 100 --rate 2 --duration 30

This will spawn 100 concurrent TCP clients, each sending 2 messages per second for 30 seconds, and then print aggregated throughput/latency stats.

We tested this for multiple different values for all the 3 versions and the results have been discussed in the report
"""

import argparse
import asyncio
import random
import statistics
import string
import time

# to generate a  random ASCII string for the messages in testing
def random_msg(size: int= 64) -> str:
    return "".join(random.choice(string.ascii_letters) for _ in range(size))

# to have a single asynchronous client for testing
class Client:
    def __init__(self, host: str, port: int, rate: float):
        self.host = host
        self.port = port
        self.rate = rate
        self.latencies_ms: list[float] = []

    # main loop whivh sends messages and gets responses
    async def run(self) -> None:
        reader, writer = await asyncio.open_connection(self.host, self.port) #open TCp connection to server
        try:
            while True:
                payload = random_msg().encode() + b"\n"
                # newline-terminated
                t0 = time.perf_counter_ns()

                writer.write(payload)
                await writer.drain()

                # wait for one line back 
                await reader.readline()
                t1 = time.perf_counter_ns()
                self.latencies_ms.append((t1 - t0) / 1_000_000) 

                await asyncio.sleep(1 / self.rate)
        except asyncio.CancelledError: #handle cancellation 
            writer.close()
            await writer.wait_closed()

# handles the actual spawning of clients and then runs them for the given duration
# collects data
async def main(args: argparse.Namespace) -> None:
    clients = [Client(args.host, args.port, args.rate)
               for _ in range(args.clients)]
    tasks   = [asyncio.create_task(c.run()) for c in clients]

    # lets full system run for this duration
    await asyncio.sleep(args.duration)

    # anmd then cancel after duration
    for t in tasks:
        t.cancel()
    await asyncio.gather(*tasks, return_exceptions=True)

    # gather stats
    all_lat = [lat for c in clients for lat in c.latencies_ms]
    if not all_lat:
        print("Nothing came back, no acknowledgement – server up?")
        return

    print(f"\nMessages sent: {len(all_lat)}")
    print(f"Mean latency:  {statistics.mean(all_lat):.2f} ms")
    print(f"Median:        {statistics.median(all_lat):.2f} ms")
    print(f"p95 latency:   {statistics.quantiles(all_lat, n=100)[94]:.2f} ms")
    print(f"p99 latency:   {statistics.quantiles(all_lat, n=100)[98]:.2f} ms")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(
        description="Async chat-server load generator"
    )
    parser.add_argument(
        "--host", default="127.0.0.1",
        help="Server hostname/IP (default: 127.0.0.1)"
    )
    parser.add_argument(
        "--port", type=int, default=2000,
        help="Server TCP port (default: 2000)"
    )
    parser.add_argument(
        "--clients", type=int, default=10,
        help="Number of concurrent clients (default: 10)"
    )
    parser.add_argument(
        "--rate", type=float, default=1.0,
        help="Messages per second, per client (default: 1.0)"
    )
    parser.add_argument(
        "--duration", type=int, default=30,
        help="Test duration in seconds (default: 30)"
    )
    args = parser.parse_args()

    asyncio.run(main(args))
