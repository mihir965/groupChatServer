#!/usr/bin/env python3
"""
bench_chat.py – chat‐server benchmarking tool

Spawns multiple concurrent TCP clients, each sending messages at a fixed
rate and awaiting an echo/broadcast round-trip. At the end it prints:
  • total messages sent
  • mean latency, p95, p99

Usage:
    python3 bench_chat.py --host 127.0.0.1 --port 2000 \
        --clients 50 --rate 2 --duration 30 --msg-size 64

For CSV output (one line):
    python3 bench_chat.py --csv --csv-header [...]
"""
import argparse, asyncio, random, string, time, statistics, csv, sys

def random_message(size: int) -> str:
    return ''.join(random.choice(string.ascii_letters) for _ in range(size))

class Client:
    def __init__(self, idx, host, port, rate, msg_size):
        self.idx = idx
        self.host = host
        self.port = port
        self.rate = rate
        self.msg_size = msg_size
        self.latencies = []
        self.sent = 0

    async def run(self, duration):
        try:
            reader, writer = await asyncio.open_connection(self.host, self.port)
        except Exception as e:
            print(f"[Client {self.idx}] Connection failed: {e}", file=sys.stderr)
            return

        start = time.perf_counter()
        next_send = start
        while True:
            now = time.perf_counter()
            if now - start > duration:
                break
            if now >= next_send:
                msg = random_message(self.msg_size).encode() + b'\n'
                t0 = time.perf_counter()
                writer.write(msg)
                await writer.drain()

                try:
                    await asyncio.wait_for(reader.readline(), timeout=5)
                except asyncio.TimeoutError:
                    print(f"[Client {self.idx}] Timeout waiting for echo", file=sys.stderr)
                    break

                t1 = time.perf_counter()
                self.latencies.append((t1 - t0) * 1000)  # ms
                self.sent += 1
                next_send += 1.0 / self.rate
            else:
                await asyncio.sleep(next_send - now)

        writer.close()
        await writer.wait_closed()

async def main(args):
    clients = [Client(i, args.host, args.port, args.rate, args.msg_size)
               for i in range(args.clients)]
    tasks = [asyncio.create_task(c.run(args.duration)) for c in clients]
    await asyncio.gather(*tasks)

    all_lat = [lat for c in clients for lat in c.latencies]
    total_msgs = sum(c.sent for c in clients)
    if all_lat:
        mean_lat = statistics.mean(all_lat)
        p95 = statistics.quantiles(all_lat, n=100)[94]
        p99 = statistics.quantiles(all_lat, n=100)[98]
    else:
        mean_lat = p95 = p99 = float('nan')

    results = {
        "clients": args.clients,
        "rate_per_client": args.rate,
        "duration_s": args.duration,
        "msg_size": args.msg_size,
        "total_msgs": total_msgs,
        "mean_lat_ms": f"{mean_lat:.2f}",
        "p95_lat_ms": f"{p95:.2f}",
        "p99_lat_ms": f"{p99:.2f}"
    }

    if args.csv:
        writer = csv.DictWriter(sys.stdout, fieldnames=results.keys())
        if args.csv_header:
            writer.writeheader()
        writer.writerow(results)
    else:
        print(f"Clients: {args.clients}, Rate/client: {args.rate} msg/s, Duration: {args.duration}s")
        print(f"Total messages sent: {total_msgs}")
        print(f"Mean latency:    {mean_lat:.2f} ms")
        print(f"95th percentile: {p95:.2f} ms")
        print(f"99th percentile: {p99:.2f} ms")

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Chat server benchmark tool")
    parser.add_argument("--host",     default="127.0.0.1", help="Server IP")
    parser.add_argument("--port",     type=int, default=2000,      help="Server port")
    parser.add_argument("--clients",  type=int, default=50,        help="Number of concurrent clients")
    parser.add_argument("--rate",     type=float, default=1.0,     help="Messages per second per client")
    parser.add_argument("--duration", type=int,   default=30,      help="Test duration in seconds")
    parser.add_argument("--msg-size", type=int,   default=64,      help="Size of each random message")
    parser.add_argument("--csv",       action="store_true",        help="Emit results as CSV")
    parser.add_argument("--csv-header",action="store_true",        help="Include CSV header row")
    args = parser.parse_args()

    asyncio.run(main(args))
