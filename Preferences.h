#ifndef _PREFERENCES_H_
#define _PREFERENCES_H_

#include <qstring.h>

class KConfig;

class Preferences {
 public:  
  Preferences();
  ~Preferences();
  void initialise(KConfig *c);
  void sync(void);

  // accessors
  int showStatus(void) {return status;};    // Draw status bar ?
  int showShadows(void) {return shadows;};  // Render shadows ?
  int miniTiles(void) {return mini;};
  int scaleMode(void) {return scale;};     // Scale background (false = tile)
  int autosave(void) {return save;};     // Scale background (false = tile)
  int showRemoved(void) {return removed;}; // show removed tiles
  int generateSolvable(void) {return solvable;}; // Generate solvable game
  QString tileset(void) {return tileFile;};   // Filename of the tile set
  QString background(void) {return backFile;}; //Filename of the background 
  QString layout(void) {return layoutFile;};  // File name of board layout
  QString defaultBackground(void); 
  QString defaultTileset(void);
  QString defaultLayout(void);
  
  // initialisers
  void setShadows(int s) {shadows = s;};
  void setMiniTiles(int s) {mini = s;};
  void setStatus(int s) {status = s;};
  void setScale(int s) {scale = s;};
  void setRemoved(int s) {removed = s;};
  void setSolvable(int s) {solvable = s;};
  void setAutosave(int s) {save = s;};
  void setTileset(QString s) {tileFile = s;};
  void setBackground(QString s) {backFile = s;};
  void setLayout(QString s) {layoutFile = s;};

 private:


  KConfig *conf;
  QString tileFile;
  QString backFile;
  QString layoutFile;
  int     status;
  int     shadows;
  int     scale;
  int	  mini;
  int     save;
  int     removed;
  int     solvable;
};

extern Preferences preferences;



#endif


