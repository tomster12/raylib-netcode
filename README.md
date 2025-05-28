# Raylib Netcode

A minimal rollback netcode prototype using an authoritative server model.  
Client developed with Raylib, both client and server written in C.  

## Core Architecture

**Authoritative Server**

- Simulates game state using confirmed or predicted inputs.
- Stores input history and state buffer for rollback.
- Resimulates on late input arrival.

**Raylib Client**

- Predict their own inputs locally.
- Simulate game logic on fixed ticks (e.g. 60Hz).
- Render at high framerate (e.g. 240Hz) using interpolation.
- Reconcile with server state via rollback if needed.

## Reference Timing Model

The rendering frames, client ticks, and server ticks are each on different intervals.  

- Simulation in the client occurs in the client ticks at the canonical frame rate.  
- The server accumulates client ticks and catches up in a single server tick.  
- The client interpolates the client ticks and takes in input on the render frames.  

```text
R = Render Frame (e.g. 240Hz)
C = Client Tick (e.g. 60Hz)
S = Server Tick (e.g. 12Hz)

R:  01 02 03 04 05 06 07 08 09 10 11 12 13 14 15 16 17 18 19 20
C:   1        2        3        4        5        6        7
S:   1                 3                 5                 7
```

In the implemented model, I am initially going to aim for R == C == 60hz due to no need for anything more than this.  
