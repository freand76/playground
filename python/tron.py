import sys
import pygame
from pygame.locals import *

def update_position(pos, direction):
    if direction == 'U':
        return (pos[0], pos[1] - 1)
    elif direction == 'D':
        return (pos[0], pos[1] + 1)
    elif direction == 'L':
        return (pos[0] - 1, pos[1])

    return (pos[0] + 1, pos[1])

def check_dead(screen, pos):
    if (screen.get_at(pos) != Color(0, 0, 0)):
        return True

    return False

RED = (255, 0, 0)
GREEN = (0, 255, 0)
BLUE = (0, 0, 255)
BLACK = (0, 0, 0)

WIDTH = 1000
HEIGHT = 700

done = False

player_one_pos = (WIDTH/4, HEIGHT/2)
player_one_dir = 'U'

player_two_pos = (WIDTH*3/4, HEIGHT/2)
player_two_dir = 'U'

right_turn = { 'U' : 'R', 'R' : 'D', 'D' : 'L', 'L' :'U'}
left_turn = { 'U' : 'L', 'L' : 'D', 'D' : 'R', 'R' :'U'}

pygame.init()
screen = pygame.display.set_mode((WIDTH, HEIGHT))
pygame.draw.rect(screen, BLUE, pygame.Rect(0,0, WIDTH, HEIGHT), 10)

while not done:
    for event in pygame.event.get():
        if event.type == pygame.QUIT:
            done = True
        elif event.type == pygame.KEYDOWN:
            if event.key == pygame.K_x:
                player_one_dir = right_turn[player_one_dir]
            if event.key == pygame.K_z:
                player_one_dir = left_turn[player_one_dir]
            if event.key == pygame.K_PERIOD:
                player_two_dir = right_turn[player_two_dir]
            if event.key == pygame.K_COMMA:
                player_two_dir = left_turn[player_two_dir]

    pygame.draw.line(screen, GREEN, player_one_pos, player_one_pos)
    pygame.draw.line(screen, RED, player_two_pos, player_two_pos)
    pygame.display.update()

    player_one_pos = update_position(player_one_pos, player_one_dir)
    player_two_pos = update_position(player_two_pos, player_two_dir)

    player_one_dead = check_dead(screen, player_one_pos)
    player_two_dead = check_dead(screen, player_two_pos)

    if player_one_dead and player_two_dead:
        print "Draw"
        sys.exit(0)
    elif player_one_dead:
        print "Two won"
        sys.exit(0)
    elif player_two_dead:
        print "One won"
        sys.exit(0)

    pygame.time.delay(20)
