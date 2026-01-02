# Distributed Job Queue (DSJQ)

A minimal RabbitMQ-inspired distributed job queue implemented in C++ to explore how real message brokers work under the hood.
This project focuses on TCP networking, concurrency, synchronization, and fault handling, rather than performance or production scale.

## Overview

This project implements a small distributed system consisting of:

- A **broker** that manages jobs and worker coordination
- Multiple **producers** that submit jobs
- Multiple **workers** that execute jobs

The system communicates over raw TCP sockets using a simple custom protocol.
All state is managed in memory and coordinated through thread-safe data structures.

The goal is to deeply understand:

- How distributed systems coordinate work
- How failures are detected and handled
- How concurrency and shared state are managed safely

## Core Features

- Multi-client TCP server
- Concurrent worker support
- Thread-safe job queue
- Job lifecycle tracking (pending → in-flight → done)
- Automatic requeue on worker disconnect
- Simple text-based protocol
- Real-time job and worker tracking

## Architecture

```text
+-------------+        +------------------+
|  Producer   | -----> |                  |
+-------------+        |                  |
                       |     Broker       |
+-------------+        |  (Job Manager)   |
|   Worker    | <----- |                  |
+-------------+        +------------------+
```

## Components

### Broker
- Central coordinator
- Owns all job state
- Assigns jobs to workers
- Requeues jobs on failure

### Producer
- Submits jobs to the broker

### Worker
- Requests jobs
- Executes tasks
- Acknowledges completion or failure

## Job Lifecycle

`PENDING` → `IN_FLIGHT` → `DONE`
            ↓
         `FAILED` → `PENDING`

- A job is **pending** when waiting in the queue
- **In-flight** when assigned to a worker
- **Done** when successfully completed
- **Failed** jobs may be retried

## Protocol Overview

All communication uses a simple newline-delimited text protocol.

### Producer Commands
`SUBMIT <payload>`

**Response:**
`JOB_ID <id>`

### Worker Commands
`REQUEST`

**Response:**
`JOB <id> <payload>`

or

`EMPTY`

`ACK <id>`
`FAIL <id>`

## Failure Handling

- Worker disconnects automatically requeue in-flight jobs
- Failed jobs can be retried
- Broker cleans up stale worker state
- System remains functional under worker churn

## Concurrency Model

- Thread-per-connection model
- Shared state protected by mutexes
- Broker acts as the single source of truth

## What This Project Is (and Isn’t)

### ✔ This project is:
- A learning-focused distributed system
- A foundation for job scheduling and message queues
- A deep dive into concurrency and networking

### ✖ This project is not:
- A production-ready message broker
- Highly optimized or persistent
- Designed for massive throughput

## Future Improvements

- **Persistent Storage**: Implement disk-based storage (e.g., Write-Ahead Log or SQLite) to ensure jobs survive broker restarts.
- **Efficient Serialization**: Migrate from a text-based protocol to a binary format like Protobuf or specialized struct packing for better performance.
- **Advanced Routing**: Add support for topics or exchange-based routing similar to RabbitMQ.
- **Security Layer**: Implement TLS/SSL encryption and simple authentication for workers and producers.
- **Message Acknowledgement Timeout**: Detect "zombie" jobs where workers hang without disconnecting, and automatically requeue them.
- **Admin CLI/Dashboard**: Create a separate client to inspect queue stats, worker count, and job throughput in real-time.
