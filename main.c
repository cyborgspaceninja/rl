/*	
	nice stuff:
		- tuts: https://tldp.org/HOWTO/NCURSES-Programming-HOWTO/printw.html
		- resizing: https://stackoverflow.com/questions/6587545/keys-not-being-interpreted-by-ncurses
		- explanation for leaks: https://stackoverflow.com/questions/32410125/valgrind-shows-memory-leaks-from-ncurses-commands-after-using-appropriate-free

	todo:
		- expandable map
		- random room gen
		- noise
		- fiender + ai
		- gjøre map-pointer til global/static istedetfor å passe den everywhere??
*/

#include <ncurses.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

//DO NOT set these smaller than your terminals dimensions! 8)
#define MAP_XSIZE 86
#define MAP_YSIZE 64
#define MY_KEY_RESIZE 410 //standard resize-val funker ikke
#define COLLIDABLES "#|¤"
#define MIN_ROOM_SIZE 4
/*
struct Actor {
	int x;
	int y;
	char c;
};
*/
void run();
int init();
int min(int a, int b);
int max(int a, int b);
int to_1d(int y, int x); //in ncurses we put the 'y' first!
void substr(char* str, char* sub , int start, int len);
void set_map(char* map);
void print_map();
void build_room(char* map, int y, int x, int h, int w);
void random_room(char* map, int y_start, int x_start, int h_max, int w_max);
void find_unobstructed_width(char* map);
void place_char(char* map, char c,int y, int x);
bool within(int pos, int bound);
bool legal_move(char* map, int y, int x);
int entity_screen_offset(int camera_pos, int entity_pos, int term_dimension);
//char get_char(char* map, int y, int x);
//void walk(char* map, char dir, struct Actor* a_ptr);

int main()	
{	
	init();
	run();
	endwin();

	return 0;
}

void run(){
	int row, col, x_offset, y_offset, close, x_player, y_player; 
	time_t t;
	
	getmaxyx(stdscr,row,col); //we get the lengths of rows and cols of our terminal
	x_player = col/2;
	y_player = row/2;

	char* row_buffer = malloc(col * sizeof(char)); //we make a variable with can hold a string as wide as our terminal
	char* map = malloc(MAP_YSIZE*MAP_XSIZE*sizeof(char));
	set_map(map);

	srand((unsigned) time(&t));
	random_room(map, MAP_YSIZE/2, MAP_XSIZE/2, 8,8);


	//find_unobstructed_width(map);
	/*
	struct Actor player = {x_player,y_player,'@'};
	struct Actor* player_ptr = &player;
	char dir = '-';
	*/
	close = y_offset = x_offset = 0;

	place_char(map,'@',y_player,x_player);

	while(!close){
		clear(); //bør kanskje egentlig bare calles hvis kameraet endrer seg og alt må oppdateres framfor et par chars her og der
	
		/*
			for each visible row on screen, we copy lines from our map of col-length into our s char-array
			our to_1d function lets us use x/y coordinates even if our map is a 1dimensional array
		*/
		for(int y = 0; y < row; y++){
			substr(
				map,
				row_buffer,
				to_1d( //transform x/y-pair to 1 dimensional coord
					y_offset+y, //y_offset holds where in the map we are looking, y variable holds the given row of our terminal we are "rendering" to
					x_offset
					),
				col
				);
			//prøv  addchstr() https://invisible-island.net/ncurses/ncurses-intro.html#tuning
			mvprintw(y,0,row_buffer); //we print a line from our map at the current terminal row we are iterating
		}
		
		//border('|', '|', '-', '-', '+', '+', '+', '+'); //idk if i should have this, fun for now ^_^

		refresh();

		switch(getch()){
			case 'q':
				close = 1;
				break;

			case MY_KEY_RESIZE:	//if you try furiously to break this by dragging your terminal around, you will ;)
				getmaxyx(stdscr,row,col);	//we again need to get set the col/row since the terminals dimensions have changed 
				free(row_buffer); //idk if this is necessary but the new size of s will be different and an escape char will soon be at a new location now
				row_buffer = malloc(col * sizeof(char)); //our buffer needs to reflect the resize as well, it must always be as wide as our terminal
				break;


			/*
				skal refaktoreres at some point
			*/
			case KEY_DOWN:
				if(legal_move(map,y_player+1,x_player)){

					if(entity_screen_offset(y_offset,y_player,row)<=0){
						y_offset = min(++y_offset, MAP_YSIZE-row);	
					}

					place_char(map,'.',y_player,x_player);
					place_char(map,'@',++y_player,x_player);
				}
				break;

			case KEY_UP:
				if(legal_move(map,y_player-1,x_player)){
					
					if(entity_screen_offset(y_offset,y_player,row)>=0){
						y_offset = max(--y_offset,0);
					}

					place_char(map,'.',y_player,x_player);
					place_char(map,'@',--y_player,x_player);
				}
				break;

			case KEY_LEFT:		
				if(legal_move(map,y_player,x_player-1)){
					
					if(entity_screen_offset(x_offset,x_player,col)>=0){
						x_offset = max(--x_offset,0);
					}

					place_char(map,'.',y_player,x_player);
					place_char(map,'@',y_player,--x_player);
				}
				break;

			case KEY_RIGHT:
				if(legal_move(map,y_player,x_player+1)){
					
					if(entity_screen_offset(x_offset,x_player,col)<=0){
						x_offset = min(++x_offset, MAP_XSIZE-col);
					}
					
					place_char(map,'.',y_player,x_player);
					place_char(map,'@',y_player,++x_player);
				}
				break;
		}
	}
	free(map);
	free(row_buffer);
}

int init(){
	initscr();				
	raw();
	keypad(stdscr, TRUE);	
	cbreak();
	noecho();	
	return 0;
}
/*
void walk(char* map, char dir, struct Actor* a_ptr){
	switch(dir){
		case('u'):
			place_char(map,'.',a_ptr->y,a_ptr->x);
			place_char(map,a_ptr->c,--a_ptr->y,a_ptr->x);
			break;
	}
}*/

/*
	if a coord is within map-arrays boundaries, and the coord is somewhere an actor can be
	then its a legal move (for now lol)
*/
bool legal_move(char* map, int y, int x){
	return 
		within(y,MAP_YSIZE) && 
		within(x,MAP_XSIZE) && 
		strchr(COLLIDABLES, map[to_1d(y,x)])==NULL;//get_char(map, y, x)) == NULL;
}

bool within(int pos, int bound){ //kanskje overill å ha en within-funksjon x)
	return pos>0 && pos<bound;
}

/*char get_char(char* map, int y, int x){
	return map[to_1d(y,x)];
}*/

void place_char(char* map, char c,int y, int x){
	map[to_1d(y,x)] = c;
}

int to_1d(int y, int x){
	return MAP_XSIZE * y + x;
}

/*
	how far a pos is from the center of the screen
	used to calculate when to track player with camera so that player will 
	remain in the center after leaving edges of the map
*/ 
int entity_screen_offset(int camera_pos, int entity_pos, int term_dimension){
	return (camera_pos + term_dimension/2)-((entity_pos - term_dimension/2) + term_dimension/2);
}

int min(int a, int b){
	return a < b ? a : b;
}

int max(int a, int b){
	return a > b ? a : b;
}

/*
	we use this function to grab a line from the map 
	(and the map is a string)
*/
void substr(char* str, char* sub , int start, int len){
    memcpy(sub, &str[start], len);
    sub[len] = '\0';
}


void find_unobstructed_width(char* map){
	/*
		we must scan the map for unobstructed lines
		from 0 to map-width at first for easyness

			-store lens in array
			-once we hit a wall and we are inside a house, store the current count, 
			-then stop counting until outside
			-then start counting until either new wall or map-end

			TODO: 
				- store lengths start, end, og y-indexen
				- vi kan adde opp lengths med samme x-index start og ende og continuous y-indekser for å finne steder å place rooms
	*/
		
	for(int y = 0; y < MAP_YSIZE; y++){
		
		bool inside = true;	//pga map-borders er '#' må denne initializes til true, ikke ideelt
		//bool wall = false;
		char last;

		for(int x = 0; x < MAP_XSIZE; x++){

			char c = map[to_1d(y,x)];

			if(c == '#' && !inside && last != '#' && x != MAP_XSIZE-1){
				inside = true;
			
			}else if(c == '#' && inside && x != MAP_XSIZE-1){
				inside = false;

			/*
				temp plasering for å se hvor rom kan spawnes
			*/
			}else if(c == '.' && !inside){
				place_char(map,'-',y,x);
			}

			last = c;
		}
	}


}

/*
	random rooms which are at least MIN_ROOM_SIZE x MIN_ROOM_SIZE
*/
void random_room(char* map, int y_start, int x_start, int h_max, int w_max){
	build_room(
		map,
		y_start,
		x_start,
		(rand() % (h_max-MIN_ROOM_SIZE)) +MIN_ROOM_SIZE,
		(rand() % (w_max-MIN_ROOM_SIZE)) +MIN_ROOM_SIZE
		);
}

void build_room(char* map, int y, int x, int h, int w){
	for(int y_i = y; y_i < y + h; y_i++){
		if(y_i == y || y_i == y+h-1){	
			for(int x_i = x; x_i < x + w; x_i++){
				place_char(map,'#',y_i,x_i);
			}		
		}else{
			place_char(map,'#',y_i,x);
			place_char(map,'#',y_i,x + w - 1);
		}
	}
}

void set_map(char* map){
	for(int y = 0; y < MAP_YSIZE; y++)
		for(int x = 0; x < MAP_XSIZE; x++)
			map[to_1d(y,x)] = (x==0 || x==MAP_XSIZE-1 || y== 0 || y == MAP_YSIZE-1) ? '#' : '.';
}

//only for debug
void print_map(char* map){
	for(int y = 0; y < MAP_YSIZE; y++){
		for(int x = 0; x < MAP_XSIZE; x++)
			printf("%c", map[to_1d(y,x)]);
		printf("\n");
	}
	free(map);
}