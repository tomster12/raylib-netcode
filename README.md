# Raylib Netcode

A minimal rollback netcode implementation with an authoritative server and a client written with Raylib.  

- https://github.com/raysan5/raylib/

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

In the implemented model I am initially going to aim for `R == C == 60hz`.
