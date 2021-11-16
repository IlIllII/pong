# ImpossiPong

**Like Pong, but impossible.**

## About

This is 60 FPS, 7-bit ASCII Pong played in your terminal. It is built on Linux and will need to be ported to play on another OS.

<div align="center">
  <img src="https://user-images.githubusercontent.com/78166995/142036252-957bbb89-0170-45d1-bdc1-3487a2c0dd66.PNG"></img>
</div>

<br>

<div align="center">
  <img src="https://user-images.githubusercontent.com/78166995/142036251-3b4932a4-9d93-48b1-a4ba-78e5fac5b20a.PNG"></img>
</div>

## To run

To play the game, just compile it with gcc and link to the pthread library:

```
gcc game.c -o pong -pthread
```

and then run the program to play:

```
./pong
```

If you can score over 100 you get a special prize :)

## Note

The game disables some terminal settings when it starts and then reenables them when it quits. If for some reason the game exits unexpectedly (it shouldn't), your terminal settings will still be disabled. To remedy this, you can just exit and reopen another terminal.
