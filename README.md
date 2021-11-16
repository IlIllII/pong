# ImpossiPong

**Like Pong, but impossible.**

## About

This is 7-bit ASCII Pong played in your terminal. It is built on Linux and will need to be ported to play on another OS.

## To run

To play the game, just compile it with gcc and link to the pthread library:

```
gcc game.c -o pong -pthread
```

and then run the program to play:

```
./pong
```

If you can score over 100 you get a special prize :).