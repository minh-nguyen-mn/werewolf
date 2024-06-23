# Werewolf

Authors: Minh Nguyen, Solomon Golden, Mai Hoang

Credit to Charlie Curtsinger for: socket, message and util.

Re-creation of the game Werewolf.
In our version of Werewolf, 7 players are required to play the game.


Connection:
--------------------------------------------------
* All code needs to be run on MathLan machines.
* The server code needs to be run first(./server), it will output a port that all users must connect to by typing: ./user ‘hostname’ ‘port #’

Game initialization:
--------------------------------------------------
* Once a user connects to the game they will be assigned a role(werewolf, witch, hunter, villager, guard, seer) randomly and a username based on when they connected(player 1-7). Roles are not broadcast to other players. There are two werewolves within the game and one of every other role.
* A brief message will be sent to all users upon connection welcoming them to the game

Game:
---------------------------------------------------
* The game starts at “night”. Each role has a special function that is active during the night, except villagers.
* The goal of the game is for the werewolves to kill all the villagers and/or have an equivalent number of werewolves to non-werewolves.
* If the non-werewolves kill all the werewolves first they win.

Gameplay:
--------------------------------------------------
	Night phase (only applicable if player with role is alive):
|||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||
		* Seer: Given a list of players and enters a player name to check their role. This                          data is only given to the seer
		* Werewolves: A short time is given for the werewolves to decide who they’d like to try and kill during this night phase. A werewolf is selected to give a non-werewolf player name.
		* Guard: The guard must select a different player to protect that night from the werewolves. If that player was selected by werewolves they will not die, at least not from that effect. Otherwise no effect
		* Witch: The witch has two potions. One can kill any player and one can save a player from a werewolf. The witch will be prompted if they’d like to save a user that is dying to a werewolf, if there is one. After that they will be prompted if they’d like to kill a player, if they do they will enter their name. Once a potion is used it cannot be used for the rest of the game.
		* Hunter: The hunter will be prompted to kill a player if they die this night phase. If the hunter dies during the night phase that player will die.
		* Villager: No effects.
		* All players who have died will have their name broadcast to all other players, even dead ones.
		* Check to see if werewolves or non-werewolves won (if either is true end game)

Day Phase:
|||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||
* Players who are alive will be given a short amount of time to discuss who they think is a werewolf.
* Players who are alive vote anonymously for who they’d like to vote to kill.
* Votes are tallied and the player with the most votes will be killed. Ties will result in no deaths.
* Check to see if werewolves or non-werewolves won (if either is true end game)
(Begin again at the start of the night phase)


Example walk-through:
-------------------------------------------------------------------
An image of a run through is in the file called Walkthrough_pic.jpeg

* All players connect. Each user is told their role.
Player 1: Seer
Player 2: Werewolf
Player 3: Werewolf
Player 4: Guard
Player 5: witch
Player 6: hunter
Player 7: village
* The seer enters Player 4
	* Server sends them: 
* Both werewolves are given 10 seconds to discuss who they’d like to kill
* One of the werewolves enter’s a user’s name: Player 6
* Guard is asked to enter a user they’d like to protect from the werewolves: Player 1
* Witch is asked if they would like to save Player 6 (y/n): n
* Witch is asked if they would like to kill anyone (y/n): y
	* Witch enters a user’s name: Player 4
* Hunter is asked if in the event they die during this night who they’d like to kill: Player 5
* The following Users have died: Player 4, Player 5, Player 6
* Game ends with werewolf victory
