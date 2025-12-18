Game Features and Mechanics

Player Control: You can move your green ship left and right using the 'A' and 'D' keys and fire bullets using the 'Space' bar .


Enemy Variety: The game features two types of enemies: standard Red Circles (10 points) and faster Blue Circles (20 points) that appear once you reach a score of 150 .


Combat System: Both the player and enemies can fire bullets. The game includes collision detection for bullets hitting enemies and for enemy bullets or ships hitting the player.


Health and Lives: The player starts with 3 lives, represented by heart icons in the HUD. After taking damage, the player receives a brief period of invulnerability.


Progression: The game difficulty increases as your score rises, with a new level triggered every 100 points.


Visual Effects: The program uses Cairo to draw a scrolling star-field background, ship shapes, and explosion animations for both small hits and the final game-over blast.

UI and Menu States

Home Screen: Displays a centered, colorful title ("SPACE SHOOTER") and a start button.


Instructions: Before starting, a countdown appears alongside a list of controls and game objectives .


HUD: A dedicated heads-up display shows your current score, level, and remaining lives.


Game States: The code manages various states including Home, Running, Paused, Instructions, and Game Over.

Implementation Details

Graphics: Built using GtkDrawingArea and Cairo for 2D vector drawing.


Timing: Uses g_timeout_add to create a consistent game loop running at approximately 60ms intervals.


Cleanup: Includes functions to safely initialize and reset game variables, timers, and UI elements during restarts.
