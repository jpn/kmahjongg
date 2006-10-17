/*
    Copyright (C) 2006 Mauricio Piacentini   <mauricio@tabuleiro.com>

    Kmahjongg is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*/

#include "GameData.h"
#include <QtDebug>


GameData::GameData () {
    //Fixed for now, TODO add new constructor with dynamic board sizes
    m_width = 32;
    m_height = 16;
    m_depth = 5;
    m_maxTiles = (m_width*m_height*m_depth)/4;

    Highlight = QByteArray(m_width*m_height*m_depth, 0);
    Mask = QByteArray(m_width*m_height*m_depth, 0);
    Board = QByteArray(m_width*m_height*m_depth, 0);
    POSITION e; //constructor initializes it to 0
    MoveList = QVector<POSITION>(m_maxTiles, e);
}

GameData::~GameData () {

}

void GameData::putTile( short e, short y, short x, UCHAR f ){
    setBoardData(e,y,x,f);
    setBoardData(e,y+1,x,f);
    setBoardData(e,y+1,x+1,f);
    setBoardData(e,y,x+1,f);
}

bool GameData::tilePresent(int z, int y, int x) {
    if ((y<0)||(x<0)||(z<0)||(y>m_height-1)||(x>m_width-1)||(z>m_depth-1)) return false;
    return(BoardData(z,y,x)!=0 && MaskData(z,y,x) == '1');
}

bool GameData::partTile(int z, int y, int x) {
    return (BoardData(z,y,x) != 0);
}

UCHAR GameData::MaskData(short z, short y, short x){
    if ((y<0)||(x<0)||(z<0)||(y>m_height-1)||(x>m_width-1)||(z>m_depth-1)) return 0;
    return Mask.at((z*m_width*m_height)+(y*m_width)+x);
}

UCHAR GameData::HighlightData(short z, short y, short x){
    if ((y<0)||(x<0)||(z<0)||(y>m_height-1)||(x>m_width-1)||(z>m_depth-1)) return 0;
    return Highlight.at((z*m_width*m_height)+(y*m_width)+x);
}

void GameData::setHighlightData(short z, short y, short x, UCHAR value) {
    if ((y<0)||(x<0)||(z<0)||(y>m_height-1)||(x>m_width-1)||(z>m_depth-1)) return ;
    Highlight[(z*m_width*m_height)+(y*m_width)+x] = value;
}

UCHAR GameData::BoardData(short z, short y, short x){
    if ((y<0)||(x<0)||(z<0)||(y>m_height-1)||(x>m_width-1)||(z>m_depth-1)) return 0;
    return Board.at((z*m_width*m_height)+(y*m_width)+x);
}

void GameData::setBoardData(short z, short y, short x, UCHAR value) {
    if ((y<0)||(x<0)||(z<0)||(y>m_height-1)||(x>m_width-1)||(z>m_depth-1)) return ;
    Board[(z*m_width*m_height)+(y*m_width)+x] = value;
}

POSITION& GameData::MoveListData(short i) {
    if ((i>=MoveList.size())|| (i<0)) {
      qDebug() << "Attempt to access GameData::MoveListData with invalid index";
      i=0 ;
    }
    return MoveList[i];
}

void GameData::setMoveListData(short i, POSITION& value){
    if ((i>=MoveList.size()) || (i<0)) return ;
    MoveList[i] = value;
}

//Game generation

// ---------------------------------------------------------
// Generate the position data for the layout from contents of Game.Map.
void GameData::generateTilePositions() {

    numTilesToGenerate = 0;

    for (int z=0; z< BoardLayout::depth; z++) {
        for (int y=0; y<BoardLayout::height; y++) {
            for (int x=0; x<BoardLayout::width; x++) {
                setBoardData(z,y,x,0);
                if (MaskData(z,y,x) == '1') {
                    tilePositions[numTilesToGenerate].x = x;
                    tilePositions[numTilesToGenerate].y = y;
                    tilePositions[numTilesToGenerate].e = z;
                    tilePositions[numTilesToGenerate].f = 254;
                    numTilesToGenerate++;
                }
            }
        }
    }
}

// ---------------------------------------------------------
// Generate the dependency data for the layout from the position data.
// Note that the coordinates of each tile in tilePositions are those of
// the upper left quarter of the tile.
void GameData::generatePositionDepends() {

    // For each tile,
    for (int i = 0; i < numTilesToGenerate; i++) {

        // Get its basic position data
        int x = tilePositions[i].x;
        int y = tilePositions[i].y;
        int z = tilePositions[i].e;

        // LHS dependencies
        positionDepends[i].lhs_dep[0] = tileAt(x-1, y,   z);
        positionDepends[i].lhs_dep[1] = tileAt(x-1, y+1, z);

        // Make them unique
        if (positionDepends[i].lhs_dep[1] == positionDepends[i].lhs_dep[0]) {
            positionDepends[i].lhs_dep[1] = -1;
        }

        // RHS dependencies
        positionDepends[i].rhs_dep[0] = tileAt(x+2, y,   z);
        positionDepends[i].rhs_dep[1] = tileAt(x+2, y+1, z);

        // Make them unique
        if (positionDepends[i].rhs_dep[1] == positionDepends[i].rhs_dep[0]) {
            positionDepends[i].rhs_dep[1] = -1;
        }

        // Turn dependencies
        positionDepends[i].turn_dep[0] = tileAt(x,   y,   z+1);
        positionDepends[i].turn_dep[1] = tileAt(x+1, y,   z+1);
        positionDepends[i].turn_dep[2] = tileAt(x+1, y+1, z+1);
        positionDepends[i].turn_dep[3] = tileAt(x,   y+1, z+1);

        // Make them unique
        for (int j = 0; j < 3; j++) {
            for (int k = j+1; k < 4; k++) {
                if (positionDepends[i].turn_dep[j] ==
                    positionDepends[i].turn_dep[k]) {
                    positionDepends[i].turn_dep[k] = -1;
                }
            }
        }

        // Placement dependencies
        positionDepends[i].place_dep[0] = tileAt(x,   y,   z-1);
        positionDepends[i].place_dep[1] = tileAt(x+1, y,   z-1);
        positionDepends[i].place_dep[2] = tileAt(x+1, y+1, z-1);
        positionDepends[i].place_dep[3] = tileAt(x,   y+1, z-1);

        // Make them unique
        for (int j = 0; j < 3; j++) {
            for (int k = j+1; k < 4; k++) {
                if (positionDepends[i].place_dep[j] ==
                    positionDepends[i].place_dep[k]) {
                    positionDepends[i].place_dep[k] = -1;
                }
            }
        }

        // Filled and free indicators.
        positionDepends[i].filled = false;
        positionDepends[i].free   = false;
    }
}

// ---------------------------------------------------------
// x, y, z are the coordinates of a *quarter* tile.  This returns the
// index (in positions) of the tile at those coordinates or -1 if there
// is no tile at those coordinates.  Note that the coordinates of each
// tile in positions are those of the upper left quarter of the tile.
int GameData::tileAt(int x, int y, int z) {

    for (int i = 0; i < numTilesToGenerate; i++) {
        if (tilePositions[i].e == z) {
            if ((tilePositions[i].x == x   && tilePositions[i].y == y) ||
                (tilePositions[i].x == x-1 && tilePositions[i].y == y) ||
                (tilePositions[i].x == x-1 && tilePositions[i].y == y-1) ||
                (tilePositions[i].x == x   && tilePositions[i].y == y-1)) {

                return i;
            }
        }
    }
    return -1;
}


// ---------------------------------------------------------
bool GameData::generateSolvableGame() {

    // Initially we want to mark positions on layer 0 so that we have only
    // one free position per apparent horizontal line.
    for (int i = 0; i < numTilesToGenerate; i++) {

        // Pick a random tile on layer 0
        int position, cnt = 0;
        do {
            position = (int) random.getLong(numTilesToGenerate);
            if (cnt++ > (numTilesToGenerate*numTilesToGenerate)) {
                return false; // bail
            }
        } while (tilePositions[position].e != 0);

        // If there are no other free positions on the same apparent
        // horizontal line, we can mark that position as free.
        if (onlyFreeInLine(position)) {
            positionDepends[position].free = true;
        }
    }

    // Check to make sure we really got them all.  Very important for
    // this algorithm.
    for (int i = 0; i < numTilesToGenerate; i++) {
        if (tilePositions[i].e == 0 && onlyFreeInLine(i)) {
            positionDepends[i].free = true;
        }
    }

    // Get ready to place the tiles
    int lastPosition = -1;
    int position = -1;
    int position2 = -1;

    // For each position,
    for (int i = 0; i < numTilesToGenerate; i++) {

        // If this is the first tile in a 144 tile set,
        if ((i % 144) == 0) {

            // Initialise the faces to allocate. For the classic
            // dragon board there are 144 tiles. So we allocate and
            // randomise the assignment of 144 tiles. If there are > 144
            // tiles we will reallocate and re-randomise as we run out.
            // One advantage of this method is that the pairs to assign are
            // non-linear. In kmahjongg 0.4, If there were > 144 the same
            // allocation series was followed. So 154 = 144 + 10 rods.
            // 184 = 144 + 40 rods (20 pairs) which overwhemed the board
            // with rods and made deadlock games more likely.
            randomiseFaces();
        }

        // If this is the first half of a pair, there is no previous
        // position for the pair.
        if ((i & 1) == 0) {
            lastPosition = -1;
        }

        // Select a position for the tile, relative to the position of
        // the last tile placed.
        if ((position = selectPosition(lastPosition)) < 0) {
            return false; // bail
        }
        if (i < numTilesToGenerate-1) {
            if ((position2 = selectPosition(lastPosition)) < 0) {
                return false; // bail
            }
            if (tilePositions[position2].e > tilePositions[position].e) {
                position = position2;  // higher is better
            }
        }

        // Place the tile.
        placeTile(position, tilePair[i % 144]);

        // Remember the position
        lastPosition = position;
    }

    // The game is solvable.
    return true;
}

// ---------------------------------------------------------
// Determines whether it is ok to mark this position as "free" because
// there are no other positions marked "free" in its apparent horizontal
// line.
bool GameData::onlyFreeInLine(int position) {

    int i, i0, w;
    int lin, rin, out;
    static int nextLeft[BoardLayout::maxTiles];
    static int nextRight[BoardLayout::maxTiles];

    /* Check left, starting at position */
    lin = 0;
    out = 0;
    nextLeft[lin++] = position;
    do {
        w = nextLeft[out++];
        if (positionDepends[w].free || positionDepends[w].filled) {
            return false;
        }
        if ((i = positionDepends[w].lhs_dep[0]) != -1) {
            nextLeft[lin++] = i;
        }
        i0 = i;
        if ((i = positionDepends[w].lhs_dep[1]) != -1 && i0 != i) {
            nextLeft[lin++] = i;
        }
    }
    while (lin > out) ;

    /* Check right, starting at position */
    rin = 0;
    out = 0;
    nextRight[rin++] = position;
    do {
        w = nextRight[out++];
        if (positionDepends[w].free || positionDepends[w].filled) {
            return false;
        }
        if ((i = positionDepends[w].rhs_dep[0]) != -1) {
            nextRight[rin++] = i;
        }
        i0 = i;
        if ((i = positionDepends[w].rhs_dep[1]) != -1 && i0 != i) {
            nextRight[rin++] = i;
        }
    }
    while (rin > out) ;

    // Here, the position can be marked "free"
    return true;
}

// ---------------------------------------------------------
int GameData::selectPosition(int lastPosition) {

    int position, cnt = 0;
    bool goodPosition = false;

    // while a good position has not been found,
    while (!goodPosition) {

        // Select a random, but free, position.
        do {
              position = random.getLong(numTilesToGenerate);
            if (cnt++ > (numTilesToGenerate*numTilesToGenerate)) {
                return -1; // bail
            }
        } while (!positionDepends[position].free);

        // Found one.
        goodPosition = true;

        // If there is a previous position to take into account,
        if (lastPosition != -1) {

            // Check the new position against the last one.
            for (int i = 0; i < 4; i++) {
                if (positionDepends[position].place_dep[i] == lastPosition) {
                    goodPosition = false;  // not such a good position
                }
            }
            for (int i = 0; i < 2; i++) {
                if ((positionDepends[position].lhs_dep[i] == lastPosition) ||
                    (positionDepends[position].rhs_dep[i] == lastPosition)) {
                    goodPosition = false;  // not such a good position
                }
            }
        }
    }

    return position;
}

// ---------------------------------------------------------
void GameData::placeTile(int position, int tile) {

    // Install the tile in the specified position
    tilePositions[position].f = tile;
    putTile(tilePositions[position]);

    //added highlight?
    //setHighlightData(E,Y,X,0);

    // Update position dependency data
    positionDepends[position].filled = true;
    positionDepends[position].free = false;

    // Now examine the tiles near this to see if this makes them "free".
    int depend;
    for (int i = 0; i < 4; i++) {
        if ((depend = positionDepends[position].turn_dep[i]) != -1) {
            updateDepend(depend);
        }
    }
    for (int i = 0; i < 2; i++) {
        if ((depend = positionDepends[position].lhs_dep[i]) != -1) {
            updateDepend(depend);
        }
        if ((depend = positionDepends[position].rhs_dep[i]) != -1) {
            updateDepend(depend);
        }
    }
}

// ---------------------------------------------------------
// Updates the free indicator in the dependency data for a position
// based on whether the positions on which it depends are filled.
void GameData::updateDepend(int position) {

    // If the position is valid and not filled
    if (position >= 0 && !positionDepends[position].filled) {

        // Check placement depends.  If they are not filled, the
        // position cannot become free.
        int depend;
        for (int i = 0; i < 4; i++) {
            if ((depend = positionDepends[position].place_dep[i]) != -1) {
                if (!positionDepends[depend].filled) {
                    return ;
                }
            }
        }

        // If position is first free on apparent horizontal, it is
        // now free to be filled.
          if (onlyFreeInLine(position)) {
              positionDepends[position].free = true;
            return;
        }

        // Assume no LHS positions to fill
        bool lfilled = false;

          // If positions to LHS
        if ((positionDepends[position].lhs_dep[0] != -1) ||
            (positionDepends[position].lhs_dep[1] != -1)) {

            // Assume LHS positions filled
            lfilled = true;

            for (int i = 0; i < 2; i++) {
                if ((depend = positionDepends[position].lhs_dep[i]) != -1) {
                    if (!positionDepends[depend].filled) {
                         lfilled = false;
                    }
                }
            }
        }

        // Assume no RHS positions to fill
        bool rfilled = false;

          // If positions to RHS
        if ((positionDepends[position].rhs_dep[0] != -1) ||
            (positionDepends[position].rhs_dep[1] != -1)) {

            // Assume LHS positions filled
            rfilled = true;

            for (int i = 0; i < 2; i++) {
                if ((depend = positionDepends[position].rhs_dep[i]) != -1) {
                    if (!positionDepends[depend].filled) {
                        rfilled = false;
                    }
                }
            }
        }

          // If positions to left or right are filled, this position
        // is now free to be filled.
          positionDepends[position].free = (lfilled || rfilled);
    }
}


// ---------------------------------------------------------
bool GameData::generateStartPosition2() {

	// For each tile,
	for (int i = 0; i < numTilesToGenerate; i++) {

		// Get its basic position data
		int x = tilePositions[i].x;
		int y = tilePositions[i].y;
		int z = tilePositions[i].e;

		// Clear Game.Board at that position
		setBoardData(z,y,x,0);

		// Clear tile placed/free indicator(s).
		positionDepends[i].filled = false;
		positionDepends[i].free   = false;

		// Set tile face blank
		tilePositions[i].f = 254;
	}

	// If solvable games should be generated,
	//if (Prefs::solvableGames()) {

		if (generateSolvableGame()) {
    		TileNum = MaxTileNum;
			return true;
		} else {
			return false;
		}
	//}

	// Initialise the faces to allocate. For the classic
	// dragon board there are 144 tiles. So we allocate and
	// randomise the assignment of 144 tiles. If there are > 144
	// tiles we will reallocate and re-randomise as we run out.
	// One advantage of this method is that the pairs to assign are
	// non-linear. In kmahjongg 0.4, If there were > 144 the same
	// allocation series was followed. So 154 = 144 + 10 rods.
	// 184 = 144 + 40 rods (20 pairs) which overwhemed the board
	// with rods and made deadlock games more likely.

	int remaining = numTilesToGenerate;
	randomiseFaces();

	for (int tile=0; tile <numTilesToGenerate; tile+=2) {
		int p1;
		int p2;

		if (remaining > 2) {
			p2 = p1 = random.getLong(remaining-2);
			int bail = 0;
			while (p1 == p2) {
				p2 = random.getLong(remaining-2);

				if (bail >= 100) {
					if (p1 != p2) {
						break;
					}
				}
				if ((tilePositions[p1].y == tilePositions[p2].y) &&
				    (tilePositions[p1].e == tilePositions[p2].e)) {
					// skip if on same y line
					bail++;
					p2=p1;
					continue;
				}
			}
		} else {
			p1 = 0;
			p2 = 1;
		}
		POSITION a, b;
		a = tilePositions[p1];
		b = tilePositions[p2];
		tilePositions[p1] = tilePositions[remaining - 1];
		tilePositions[p2] = tilePositions[remaining - 2];
		remaining -= 2;

		getFaces(a, b);
		putTile(a);
		putTile(b);
	}

    TileNum = MaxTileNum;
	return 1;
}

void GameData::getFaces(POSITION &a, POSITION &b) {
	a.f = tilePair[tilesUsed];
	b.f = tilePair[tilesUsed+1];
	tilesUsed += 2;

	if (tilesUsed >= 144) {
		randomiseFaces();
	}
}

void GameData::randomiseFaces() {
	int nr;
	int numAlloced=0;
	// stick in 144 tiles in pairsa.

        for( nr=0; nr<9*4; nr++)
		tilePair[numAlloced++] = TILE_CHARACTER+(nr/4); // 4*9 Tiles
        for( nr=0; nr<9*4; nr++)
		tilePair[numAlloced++] = TILE_BAMBOO+(nr/4); // 4*9 Tiles
        for( nr=0; nr<9*4; nr++)
		tilePair[numAlloced++] = TILE_ROD+(nr/4); // 4*9 Tiles
        for( nr=0; nr<4;   nr++)
		tilePair[numAlloced++] = TILE_FLOWER+nr;         // 4 Tiles
        for( nr=0; nr<4;   nr++)
		tilePair[numAlloced++] = TILE_SEASON+nr;         // 4 Tiles
        for( nr=0; nr<4*4; nr++)
		tilePair[numAlloced++] = TILE_WIND+(nr/4);  // 4*4 Tiles
        for( nr=0; nr<3*4; nr++)
		tilePair[numAlloced++] = TILE_DRAGON+(nr/4);     // 3*4 Tiles


	//randomise. Keep pairs together. Ie take two random
	//odd numbers (n,x) and swap n, n+1 with x, x+1

	int at=0;
	int to=0;
	for (int r=0; r<200; r++) {


		to=at;
		while (to==at) {
			to = random.getLong(144);

			if ((to & 1) != 0)
				to--;

		}
		UCHAR tmp = tilePair[at];
		tilePair[at] = tilePair[to];
		tilePair[to] = tmp;
		tmp = tilePair[at+1];
		tilePair[at+1] = tilePair[to+1];
		tilePair[to+1] = tmp;


		at+=2;
		if (at >= 144)
			at =0;
	}

	tilesAllocated = numAlloced;
	tilesUsed = 0;
}


