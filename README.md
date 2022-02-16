# numa-pingpong
Timing of core-to-core synchronization ping-pong on NUMA and generic multi-core platforms using different methods.

# Requirement
- Ubuntu 20.10 (or later)
- g++-8
# Build 
```
cd src && make
```

# Run & display
```
cd script
./run-numa-pingpong.sh {condvar, pipe, tkt-lock, atomic-wait, futex, atomic}
./draw-ppong-cmp.sh ppong*.dat
```
