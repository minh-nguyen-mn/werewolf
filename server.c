/*-----------------------------------------LIBRARY-----------------------------------------*/


#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "socket.h"
#include "message.h"
#include "util.h"


/*-----------------------------------------MACROS-----------------------------------------*/


#define ALIVE 0
#define DEAD 1
#define DISCONNECTED 2
#define MAX_NAME_LEN 9
#define MAX_ROLE_LEN 12
#define MAX_WEREWOLF_COUNT 2
#define DISCUSSION_TIME_NIGHT 10000
#define DISCUSSION_TIME_DAY 20000
#define USERS 7 // Number of users connected this specific client


/*-----------------------------------------GLOBAL VALUES-----------------------------------------*/


bool witch_kill = true; // whether the witch has used her kill potion
bool witch_save = true; // whether the witch has used her save potion

// All 7 roles and their availability
char roles[][USERS][MAX_ROLE_LEN] = {{"werewolf", "werewolf", "guard", "witch", "hunter", "seer", "villager"},
                                     {"available", "available", "available", "available", "available", "available", "available"}};

// All player names
char names[][MAX_NAME_LEN] = {"Player 1", "Player 2", "Player 3", "Player 4", "Player 5", "Player 6", "Player 7"};

// struct that stores user's info
typedef struct user
{
  char player_name[MAX_NAME_LEN];
  int socket;
  char role[MAX_ROLE_LEN];
  int status;        // whether they're dead or alive
  int votes_against; // tally of their votes during the day function
  char *message;
} users_t;

// Array of all users
users_t user_lst[USERS];

// Dictates who to receive and broadcast message to
// When active_roles is 'public', everyone gets to talk
// Otherwise, active_roles will be modified to reflect who gets to communicate
char *active_roles;


/*-----------------------------------------FUNCTIONS HEADERS-----------------------------------------*/


/*----------Connections----------*/

// Thread function for each player that check active status then receive message if appropriate
void *user_input(void *user_info);

// Loop and accept connections from 7 users before starting the game
void accept_connections(int socket);

/*----------Messages----------*/

// Check whether message was sent, if not, call fail_message
void send_safe_message(users_t user_x, char *message);

// Check whether user has disconnected, if so, kill them and mute them
void fail_message(users_t *user_to_kill);

// Receive miscellaneous from users and discard them
void dump();

/*----------User Set-up and Check----------*/

// Send welcoming messages and inform users of their name and roles
void welcome_user(int user_port, int i);

// Function to determine if a given name is an alive player name
bool check_name(char *name);

/*----------Status Updates----------*/

/* Check game's current state to see if they match any of the ending criteria
   Returns true if the game continues, false if otherwise. */
bool check_game_status();

// Inform users of what happened last night, and whether there are any deaths
void night_status_update(char *witch_k, char *werewolf_k, char *hunter_k);

/*----------Role Functions----------*/

/* Calls seer(), then prompt the werewolves to kill a non-werewolf player, give them 10s to discuss 
   and one player will be called upon to make a decision
   Returns player to be killed
   Notes: they cannot kill themselves */
char *werewolf_night_func();

/* Calls the guard and prompt them to save one person
   If they happen to save a dying person, an empty string will be returned
   Else, return the person killed by the werewolves
   Notes: the guard cannot save themself and does not know who was killed by the wolves */
char *guard_night_func();

/* Inform the witch of the dying person, if any, then ask whether they want to save
   Returns appropriate dying player if they are not saved
   Otherwise, return an empty string if they are saved
   Notes: the witch can only save once */
char *witch_night_func_save();

/* Ask the witch whether they want to kill someone
   If yes, return the player's name, else return an empty string
   Notes: the witch can only kill once */
char *witch_night_func_kill();

// Prompt the seer to see one player's role
void seer();

/* Prompt the hunter to pick one person to die with them if they are killed
   If they are killed by either the witch or the werewolves, return the name of the player they pick 
   Else, return an empty string */
char *hunter_func(char *witch_s, char *witch_k);

/*----------Day Phase Function----------*/

// Prompt all users to discuss then take turn to vote on one player to be killed
void day_func();


/*-----------------------------------------FUNCTIONS-----------------------------------------*/


/*-------------------------Connections-------------------------*/


// Thread function for each player that check active status then receive message if appropriate
void *user_input(void *user_info)
{
  users_t *my_user = (users_t *)user_info;

  // Wait on message from user
  while (true)
  { 
    char *message = receive_message(my_user->socket);
    if (message == NULL)
    {
      perror("Failed to read message from client");
      // exit(EXIT_FAILURE);
    }

    for (int i = 0; i < USERS; i++)
    {
      // Check if active_roles is appropriate
      if (strcmp(user_lst[i].player_name, my_user->player_name) != 0 &&
          ((strcmp(user_lst[i].role, my_user->role) == 0) || strcmp("public", active_roles) == 0) && my_user->status == ALIVE)
      {
        send_safe_message(user_lst[i], my_user->player_name);
        send_safe_message(user_lst[i], ": ");
        send_safe_message(user_lst[i], message);
        send_safe_message(user_lst[i], "\n");
      }
    }
    free(message);
  } // while loop

  return NULL;

} // user_input



// Loop and accept connections from 7 users before starting the game
void accept_connections(int server_socket_fd)
{
  int i = 0;

  // Loop till all 7 players are connected
  while (i < USERS)
  {
    //  create socket and accept connection
    int client_socket_fd = server_socket_accept(server_socket_fd);
    if (client_socket_fd == -1) // Wait for a client to connect
    {
      perror("accept failed");
      exit(EXIT_FAILURE);
    }

    // Set up player's initial status
    strcpy(user_lst[i].player_name, names[i]);
    user_lst[i].socket = client_socket_fd;
    user_lst[i].status = ALIVE;
    user_lst[i].votes_against = 0;
    welcome_user(client_socket_fd, i);

    i++;
  } // while loop

} // accept_connecttions



/*-------------------------Messages-------------------------*/



// Check whether message was sent, if not, call fail_message
void send_safe_message(users_t receiver, char *message)
{
  int rc = send_message(receiver.socket, message);
  if (rc == -1)
  {
    fail_message(&receiver);
  }
} // send_safe_messages



// Check whether user has disconnected, if so, kill them and mute them
void fail_message(users_t *user_to_kill)
{
  // Check whether user is connected
  if (user_to_kill->status != DISCONNECTED)
  {
    // Change the given user's status to be DISCONNECTED
    user_to_kill->status = DISCONNECTED;
    // Transmit a message to all other users in the network that our given user has disconnected and run again if a message fails cause another user disconnected
    for (int i = 0; i < USERS; i++)
    {
      if (user_lst[i].status != DISCONNECTED)
      {
        int rc = send_message(user_lst[i].socket, user_to_kill->player_name);
        rc = send_message(user_lst[i].socket, " has disconnected and will be considered dead for the rest of the game, if not already.");
        if (rc == -1)
        {
          fail_message(&user_lst[i]);
        }
      }
    }
  } // for loop
} // fail_message



// Receive all the miscellaneous from users and discard them
// This is neccessary since every users' thread listen for their messages all the time and sometimes unwanted messages get mixed in
void dump()
{
  pthread_t thread[USERS];
  active_roles = "dump time";
  for (int z = 0; z < USERS; z++)
  {
    if (pthread_create(&thread[z], NULL, user_input, &user_lst[z]) != 0) // create the thread for user_input for the new connection
    {
      perror("failed to create thread.\n");
      exit(1);
    }
  }
  int wakeup_time = time_ms() + 500; // Give it 0.5s to dump
  while ((int)time_ms() < wakeup_time)
    ;
  for (int i = 0; i < USERS; i++)
  {
    if (pthread_cancel(thread[i]) != 0)
    {
      perror("Failed to join thread.\n");
      exit(1);
    }
  }
} // dump



/*-------------------------User Set-Up and Name Check-------------------------*/



// Send welcoming messages and inform users of their name and roles
void welcome_user(int user_port, int i)
{
  // Welcome message and assign username
  int rc = send_message(user_port, "Hello Player!\nWelcome to Werewolf!\nThe horror will start soon but for now. Your username will be: ");
  if (rc == -1)
  {
    perror("Failed to send message to client");
    exit(EXIT_FAILURE);
  }
  rc = send_message(user_port, user_lst[i].player_name);
  send_message(user_port, "\n");
  if (rc == -1)
  {
    perror("Failed to send message to client");
    exit(EXIT_FAILURE);
  }

  // Assign their role 
  while (true)
  {
    time_t t;
    srand(time(&t));
    int role_index = rand() % 7;
    if (strcmp(roles[1][role_index], "available") == 0)
    {
      strcpy(roles[1][role_index], "taken");
      strcpy(user_lst[i].role, roles[0][role_index]);
      break;
    }
    else
      continue;
  } // while loop

  send_message(user_port, "Your role is: ");
  send_message(user_port, user_lst[i].role);
  send_message(user_port, "\n");
  send_message(user_port, "The Game will start shortly!\n");

} // welcome_user



// Function to determine if a given name is an alive player name
bool check_name(char *name)
{
  for (int i = 0; i < USERS; i++)
  {
    if (strcmp(name, user_lst[i].player_name) == 0 && user_lst[i].status == ALIVE)
    {
      return true;
    }
  }
  return false;
} // check_name



/*-------------------------Status Updates-------------------------*/



/* Check game's current state to see if they match any of the ending criteria
   Returns true if the game continues, false if otherwise. */
bool check_game_status()
{

  int aliveCount = 0;
  int werewolfCount = 0;

  // Tally alive werewolves and villagers
  for (int i = 0; i < USERS; i++)
    if (user_lst[i].status == ALIVE)
    {
      aliveCount++;
      if (strcmp(user_lst[i].role, "werewolf") == 0)
        werewolfCount++;
    }

  // If everyone is dead
  if (werewolfCount == 0 && aliveCount == 0)
  {
    for (int i = 0; i < USERS; i++)
      send_safe_message(user_lst[i], "No one wins! All are dead.");
    return false;
  }

  // If all werewolves are dead
  else if (werewolfCount == 0)
  {
    for (int i = 0; i < USERS; i++)
      send_safe_message(user_lst[i], "Villagers win! All werewolves are dead.");
    return false;
  }

  // If werewolves >= villagers
  else if (werewolfCount >= (aliveCount + 1) / 2)
  {
    for (int i = 0; i < USERS; i++)
      send_safe_message(user_lst[i], "Werewolves win! Werewolves are at least half of the remainings.");
    return false;
  }

  // The game continues otherwise
  return true;

} // check_game_status



// Inform users of what happened last night, and whether there are any deaths
void night_status_update(char *witch_k, char *werewolf_k, char *hunter_k)
{
  // Loop through all users
  for (int i = 0; i < USERS; i++)
  {
    // Check and mark whether user is dead
    if (strcmp(hunter_k, user_lst[i].player_name) == 0 || strcmp(witch_k, user_lst[i].player_name) == 0 || strcmp(werewolf_k, user_lst[i].player_name) == 0)
    {
      user_lst[i].status = DEAD;
    }

    // If nobody dies
    if ((strcmp(witch_k, "") == 0) && (strcmp(werewolf_k, "") == 0) && (strcmp(hunter_k, "") == 0)) {
      send_safe_message(user_lst[i], "It has been a peaceful night, nobody dies.\n");
      continue;
    }

    // If there are deaths, send users the deaths
    send_safe_message(user_lst[i], "The following users died: \n");
    if (strcmp(witch_k, "") != 0)
    {
      send_safe_message(user_lst[i], witch_k);
      send_safe_message(user_lst[i], "\n");
    }
    if (strcmp(werewolf_k, "") != 0)
    {
      send_safe_message(user_lst[i], werewolf_k);
      send_safe_message(user_lst[i], "\n");
    }
    if (strcmp(hunter_k, "") != 0)
    {
      send_safe_message(user_lst[i], hunter_k);
      send_safe_message(user_lst[i], "\n");
    }

  } //for loop

} // night_status_update



/*-------------------------Role Functions-------------------------*/



/* Calls seer(), then prompt the werewolves to kill a non-werewolf player, give them 10s to discuss 
   and one player will be called upon to make a decision
   Notes: they cannot kill themselves */
char *werewolf_night_func()
{
  seer(); // seer is called
  dump();

  // Make werewolves able to communicate
  active_roles = "werewolf";
  // Find the two werewolves
  int werewolves[MAX_WEREWOLF_COUNT];
  werewolves[0] = -1;
  werewolves[1] = -1;
  int w = 0;
  pthread_t thread[MAX_WEREWOLF_COUNT];

  // Find the werewolves and send them a list of all the non-werewolves
  for (int i = 0; i < USERS; i++)
  {
    if (strcmp(user_lst[i].role, "werewolf") == 0 && user_lst[i].status == ALIVE)
    {
      send_safe_message(user_lst[i], "You will be given 10 seconds to decide amongst yourselves who you would like to kill.\n Here are the users you may kill.\n");
      werewolves[w] = i;
      if (pthread_create(&thread[w], NULL, user_input, &user_lst[werewolves[w]]) != 0) // create the thread for user_input for the new connection
      {
        perror("failed to create thread.\n");
        exit(1);
      }
      w++;

      // Send them the alive players
      for (int z = 0; z < USERS; z++)
      {
        if (strcmp(user_lst[z].role, "werewolf") != 0 && (user_lst[z].status == ALIVE))
        {
          send_safe_message(user_lst[i], user_lst[z].player_name);
          send_safe_message(user_lst[i], "\n");
        }
      } // for loop for alive players
    }    
  } // for loop to prompt the killing


  // Give 10 seconds for the werewolves to discuss
  int wakeup_time = time_ms() + DISCUSSION_TIME_NIGHT;
  char *message;
  while ((int)time_ms() < wakeup_time)
    ;
  if (werewolves[0] != -1 && pthread_cancel(thread[0]) != 0) // create the thread for user_input for the new connection
  {
    perror("failed to cancel thread.\n");
    exit(1);
  }
  if (werewolves[1] != -1 && pthread_cancel(thread[1]) != 0) // create the thread for user_input for the new connection
  {
    perror("failed to cancel thread.\n");
    exit(1);
  }


  // Make sure no one is able to send/receive messages
  active_roles = "Everyone shut up";
  int alive_werewolf = 0;
  if (werewolves[alive_werewolf] == -1)
    alive_werewolf++;
  send_safe_message(user_lst[werewolves[alive_werewolf]], "Time is up. Choose one player to slaughter.\n");
  if (alive_werewolf == 0)
    send_safe_message(user_lst[werewolves[1]], "The other werewolf will choose someone to die\n");
  else
    send_safe_message(user_lst[werewolves[0]], "The other werewolf will choose someone to die\n");


  // Find out who the werewolves wanna vote for and validate that input
  message = receive_message(user_lst[werewolves[alive_werewolf]].socket);
  while (!check_name(message) || strcmp(user_lst[werewolves[0]].player_name, message) == 0 || strcmp(user_lst[werewolves[1]].player_name, message) == 0)
  {
    send_safe_message(user_lst[werewolves[alive_werewolf]], "You have entered an invalid input. Please try again: \n");
    char *temp = message;
    free(temp);
    message = receive_message(user_lst[werewolves[alive_werewolf]].socket);
  }

  return message;
} // were_wolf_night_func



/* Calls werewolf_night_func()then prompt the guard to save one person
   If they save a dying person, an empty string will be returned
   Else, return the person killed by the werewolves
   Notes: the guard cannot save themself and does not know who was killed by the wolves */
char *guard_night_func()
{
  dump();
  char *name = werewolf_night_func(); // Werewolves are called here

  for (int i = 0; i < USERS; i++)
  {
    // Find the guard
    if (strcmp(user_lst[i].role, "guard") == 0 && user_lst[i].status == ALIVE)
    {
      send_safe_message(user_lst[i], "Choose a player you would like to save:\n");
      for (int z = 0; z < USERS; z++) // Send a list of alive players
      {
        if (i != z && (user_lst[z].status == ALIVE))
        {
          send_safe_message(user_lst[i], user_lst[z].player_name);
          send_safe_message(user_lst[i], "\n");
        }
      }

      // Receive a choice and validate it
      char *message = receive_message(user_lst[i].socket);
      while (!check_name(message) || strcmp(user_lst[i].player_name, message) == 0)
      {
        send_safe_message(user_lst[i], "You have entered an invalid input. Please try again.\n");
        char *temp = message;
        free(temp);
        message = receive_message(user_lst[i].socket);
      }

      // If they save the player the werewolves killed, return an empty string
      if (strcmp(name, message) == 0)
      {
        free(message);
        free(name);
        return "";
      }
      free(message);
    }
  } // for loop

  // If they save someone who wasn't killed by werewolves, return the name of the player killed by werewolves
  return name;
} // guard_night_func



/* Inform the witch of the dying person, if any, then ask whether they want to save
   Returns appropriate dying player if they are not saved
   Otherwise, return an empty string if they are saved
   Notes: the witch can only save once */
char *witch_night_func_save()
{
  dump();

  // Gets index of witch player
  int index = 0;
  for (; index < USERS; index++)
    if (strcmp(user_lst[index].role, "witch") == 0)
      break;

  // Sends witch information of potential death
  char *dying = guard_night_func();
  char message[50];
  strcpy(message, dying);
  if (!strlen(dying))
    strcat(message, "No one");
  strcat(message, " is dying.");
  send_safe_message(user_lst[index], message);
  send_safe_message(user_lst[index], "\n");

  // If there is a potential death
  if (strlen(dying))
  {
    // If save potion is unavailable, they can't use it
    if (!witch_save)
      send_safe_message(user_lst[index], "You used your save potion.\n");
    // If save potion is available, ask them if they want to save
    else
    {
      send_safe_message(user_lst[index], "Do you want to save? (y/n)\n");
      char *choice = receive_message(user_lst[index].socket);
      if (choice != NULL)
      {
        // If they save, return an empty string
        if (strcmp(choice, "y") == 0)
        {
          witch_save = false;
          free(choice);
          free(dying);
          return "";
        }
        free(choice);
      }
    }
  }

  // If the witch does not save, return the dying player
  return dying;
} // witch_night_func_save


/* Ask the witch whether they want to kill someone
   If yes, return the player's name, else return an empty string
   Notes: the witch can only kill once */
char *witch_night_func_kill()
{
  dump();

  // Gets index of witch player
  int index = 0;
  for (; index < USERS; index++)
    if (strcmp(user_lst[index].role, "witch") == 0)
      break;

  // If kill potion is unavailable, they can't use it
  if (!witch_kill)
    send_safe_message(user_lst[index], "You used your kill potion.\n");

  // If kill potion is available
  else
  { // Ask whether they want to kill
    send_safe_message(user_lst[index], "Do you want to kill? (y/n)\n");
    char *choice = receive_message(user_lst[index].socket);
    if (choice != NULL)
    {
      // If they kill, ask for a name and validae
      if (strcmp(choice, "y") == 0)
      {
        send_safe_message(user_lst[index], "Who do you want to kill?\n");
        char *dying;
        while (true)
        {
          dying = receive_message(user_lst[index].socket);
          if (!check_name(dying))
          {
            send_safe_message(user_lst[index], "Invalid username, please re-enter.)\n");
            char *temp = dying;
            free(temp);
            continue;
          }
          else
          {
            int i = 0;
            for (; i < USERS; i++)
            {
              if (strcmp(dying, user_lst[i].player_name) == 0)
                break;
            }
            if (user_lst[i].status != ALIVE)
            {
              send_safe_message(user_lst[index], "User is no longer with us, please re-enter.\n");
              char *temp2 = dying;
              free(temp2);
              continue;
            }
          }
          break; 
        } // while loop

        // Return player killed by the witch
        witch_kill = false;
        free(choice);
        return dying;
      } // if kill

      free(choice);

    } // if (choice != NULL)
  }
  // Return an empty string if no killing happened
  return "";
} // witch_night_func_kill



// Prompt the seer to see one player's role
void seer()
{
  // Mute other players
  active_roles = "Shhhhhhhh";

  // Find the seer
  for (int i = 0; i < USERS; i++)
  {
    if (strcmp(user_lst[i].role, "seer") == 0)
    {
      if (user_lst[i].status != ALIVE)
        return;

      // Ouput all the options, not themself
      send_safe_message(user_lst[i], "Type the name of a player you would like to check the role of: \n");
      for (int z = 0; z < USERS; z++)
      {
        if (strcmp(user_lst[i].player_name, user_lst[z].player_name) != 0 && user_lst[z].status == ALIVE)
        {
          send_safe_message(user_lst[i], user_lst[z].player_name);
          send_safe_message(user_lst[i], "\n");
        }
      }

      // Get input from the seer to see who's role they'd like to see.
      // Validation check to see if they want to check themselves or someone who doesnt exist
      char *mess = receive_message(user_lst[i].socket);
      dump();
      while (!check_name(mess) || strcmp(user_lst[i].player_name, mess) == 0)
      {
        send_safe_message(user_lst[i], "You have entered an invalid input, try again: ");
        char *temp = mess;
        free(temp);
        mess = receive_message(user_lst[i].socket);
      }

      // Find the designated user chosen by the seer and send them their role.
      for (int z = 0; z < USERS; z++)
      {
        if (strcmp(user_lst[z].player_name, mess) == 0)
        {
          send_safe_message(user_lst[i], user_lst[z].role);
          send_safe_message(user_lst[i], "\n");
        }
      }
      free(mess);
    }
  }
  return;
} // seer



/* Prompt the hunter to pick one person to die with them if they are killed
   If they are killed by either the witch or the werewolves, return the name of the player they pick 
   Else, return an empty string */
char *hunter_func(char *witch_s, char *witch_k)
{
  dump();

  // Gets index of hunter player
  int index = 0;
  int i = 0;
  char *dead_guy;
  for (; index < USERS; index++)
    if (strcmp(user_lst[index].role, "hunter") == 0)
    {
      if (user_lst[index].status != ALIVE)
      {
        return "";
      }
      break;
    }

  // Prompt the choice
  send_safe_message(user_lst[index], "The night has arrived. You now have a chance to mark an unfortunate victim who will join you in Death if the chance ever arise!\n");

  // Receive the choice and validate
  while (true)
  {
    dead_guy = receive_message(user_lst[index].socket);
    if (!check_name(dead_guy))
    {
      send_safe_message(user_lst[index], "Invalid username, please re-enter.\n");
      char *temp = dead_guy;
      free(temp);
      continue;
    }
    else
    {
      for (; i < USERS; i++)
      {
        if (strcmp(dead_guy, user_lst[i].player_name) == 0)
          break;
      }
      if (user_lst[i].status != ALIVE)
      {
        send_safe_message(user_lst[index], "User is no longer with us, please re-enter.\n");
        char *temp2 = dead_guy;
        free(temp2);
        continue;
      }
    }
    break;
  } // while loop

  // If hunter is killed during the night, return his choice
  if (strcmp(witch_k, user_lst[index].player_name) == 0 || strcmp(witch_s, user_lst[index].player_name) == 0)
  {
    return dead_guy;
  }

  // Else, return an empty string
  free(dead_guy);
  return "";
}



/*-------------------------Day Phase Function------------------------*/



// Prompt all users to discuss then take turn to vote on one player to be killed
void day_func()
{

  active_roles = "public";
  pthread_t thread[USERS];

  // Prompt all user to discuss
  for (int z = 0; z < USERS; z++)
  {
    send_safe_message(user_lst[z], "You will be given 20 seconds to discuss who you would like to vote out.\n");
    if (pthread_create(&thread[z], NULL, user_input, &user_lst[z]) != 0) // create the thread for user_input for the new connection
    {
      perror("failed to create thread.\n");
      exit(1);
    }
  } // for loop

  int wakeup_time = time_ms() + DISCUSSION_TIME_DAY;
  while ((int)time_ms() < wakeup_time)
    ;
  for (int i = 0; i < USERS; i++)
  {
    if (pthread_cancel(thread[i]) != 0)
    {
      perror("Failed to join thread.\n");
      exit(1);
    }
  } // for loop

  // User takes turn to vote
  active_roles = "shut up";
  for (int z = 0; z < USERS; z++)
  {
    if (user_lst[z].status == ALIVE)
    {
      send_safe_message(user_lst[z], "Please enter a player's name:\n");
      char *message = receive_message(user_lst[z].socket);
      while (!check_name(message))
      {
        send_safe_message(user_lst[z], "Invalid input, enter a real player's name who is alive:\n");
        char *temp = message;
        free(temp);
        message = receive_message(user_lst[z].socket);
      }
      for (int i = 0; i < USERS; i++)
      {
        if (strcmp(user_lst[i].player_name, message) == 0)
        {
          user_lst[i].votes_against++;
        }
      }
      free(message);
    }
  } // for loop

  // tally votes
  bool tie = false;
  users_t *to_die = &user_lst[0];
  for (int z = 1; z < USERS; z++)
  {
    if (to_die->votes_against < user_lst[z].votes_against)
    {
      tie = false;
      to_die = &user_lst[z];
    }
    else if (to_die->votes_against == user_lst[z].votes_against)
    {
      tie = true;
    }
  }

  // If it's a tie, nobody dies
  if (tie)
  {
    for (int i = 0; i < USERS; i++)
    {
      send_safe_message(user_lst[i], "There was a tie, no one will die.\n");
    }
  }
  else // else kill off the player with the most votes_against
  {
    to_die->status = DEAD;
    for (int i = 0; i < USERS; i++)
    {
      send_safe_message(user_lst[i], to_die->player_name);
      send_safe_message(user_lst[i], " has been voted out. They were a: ");
      send_safe_message(user_lst[i], to_die->role);
      send_safe_message(user_lst[i], "\n");
    }
  }

  // Set votes_against back to 0
  for (int i = 0; i < USERS; i++)
  {
    user_lst[i].votes_against = 0;
  }
} // day_func



/*-----------------------------------------MAIN-----------------------------------------*/


int main()
{
  //  Set up a server socket to accept incoming connections
  unsigned short port = 0;
  int server_socket_fd = server_socket_open(&port);
  if (server_socket_fd == -1)
  {
    perror("Server socket was not opened");
    exit(EXIT_FAILURE);
  }

  // Listen for connections, with a maximum of 1 connection in queue
  if (listen(server_socket_fd, 1)) 
  {
    perror("listen failed");
    exit(EXIT_FAILURE);
  }

  printf("SERVER PORT: %u\n", port);

  // Thread is created to begin accepting new connections
  accept_connections(server_socket_fd);
  active_roles = "Shhhhh";

  while (check_game_status()) // Game loop: While ending state is not reached, the game continues
  {
    // All roles function is called in order of seer, werewolves, guard, witch, and hunter
    // Seer is called within werewolves
    // Werewolves are called within guard
    // Guard is called within witch_save
    // Witch_kill calls itself
    // Hunter calls itself
    char *werewolf_k = witch_night_func_save(); 
    char *witch_k = witch_night_func_kill();
    char *hunter_k = hunter_func(werewolf_k, witch_k);
    
    night_status_update(witch_k, werewolf_k, hunter_k);

    // Free memory malloc-ed by receive message
    char *werewolf_temp = werewolf_k;
    char *witch_temp = witch_k;
    char *hunter_temp = hunter_k;
    if (strcmp(werewolf_temp, "") != 0)
      free(werewolf_temp);
    if (strcmp(witch_temp, "") != 0)
      free(witch_temp);
    if (strcmp(hunter_temp, "") != 0)
      free(hunter_temp);
    
    // If ending state is not reached, move on to day phase
    if (check_game_status())
    {
      dump();
      day_func();
    } // Night phase follows if ending state is not reached

    else
    {
      break;
    }
  }
}