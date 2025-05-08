# Concurrent Chat Server Implementations

This repository contains multiple implementations of a concurrent chat server in C, with each version demonstrating a different concurrency model.

## Branches

| Branch Name       | Implementation Type         |
|-------------------|-----------------------------|
| `multiThreaded`| Thread-per-client           |
| `select`      | Event-driven using `select` |
| `pool`      | Asymmetric multi-process event-driven (thread pool) |

## Common Structure

Each branch includes:
- `server.c` – entry point of the server
- `utils.c`, `utils.h` – common networking and utility functions
- `blocking_io.c`, `blocking_io.h` – simulates blocking disk I/O
- `client.c` – simple test client

---

## How to Build & Run

Use the following `clang` command to compile:

```bash
clang server.c utils.c blocking_io.c -o server -lpthread

