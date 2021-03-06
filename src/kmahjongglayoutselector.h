/*
    KMahjonggLayoutSelector
    Part of kmahjongg, the classic mahjongg game for KDE

    Copyright (C) 2007 Mauricio Piacentini   <mauricio@tabuleiro.com>

    This program is free software; you can redistribute it and/or modify
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

#ifndef KMAHJONGGLAYOUTSELECTOR_H
#define KMAHJONGGLAYOUTSELECTOR_H

// KDE
#include <KConfigSkeleton>

// KMahjongg
#include "ui_gametype.h"

class GameView;
class GameData;
class GameScene;
class KMahjonggLayout;

class KMahjonggLayoutSelector : public QWidget, public Ui::KMahjonggLayoutSelector
{
    Q_OBJECT
public:
    explicit KMahjonggLayoutSelector(QWidget * parent, KConfigSkeleton * config);

public slots:
    void layoutChanged();
    void useRandomLayoutToggled(bool);

private:
    void setupData(KConfigSkeleton * config);

    QMap<QString, KMahjonggLayout *> layoutMap;
    GameScene * m_gameScene;
    GameData * m_gameData;
    GameView * m_gameView;
};

#endif
