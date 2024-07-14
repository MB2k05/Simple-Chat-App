# Basic Command Line Chat Application

A simple command line based chatting application implemented in C that utilises socket programming. This application consists of a server that can handle multiple clients simultaneously. Clients can send and receive messages in real time.

## Features

* Multi-client handling using `select()`.

* Broadcasting messages to all connected clients

* Clients can see their own messages and messages from others.

## Requirements

* GCC Compiler

* POSIX compliant system

## Getting started

### Clone the repository

### Compile the code

* gcc -o server server.c
* gcc -o client client.c -lpthread

### Run the server

* ./server

### Run the client

* ./client


You can run multiple clients to simulate a chat environment. Opens several terminal windows and run the client in each.

## Contributions

Contributions are more than welcome. Please feel free to submit a pull request!