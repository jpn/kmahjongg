/* Copyright (C) 2012 Christian Krippendorf <Coding@Christian-Krippendorf.de>
 *
 * Kmahjongg is free software; you can redistribute it and/or modify it under the terms of the GNU
 * General Public License as published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without
 * even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with this program; if
 * not, write to the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA. */

// own
#include "gameview.h"

// Qt
#include <QMouseEvent>
#include <QResizeEvent>

// KDE
#include <KLocalizedString>
#include <KMessageBox>
#include <KRandom>

// KMahjongg
#include "demoanimation.h"
#include "gamedata.h"
#include "gameitem.h"
#include "gamescene.h"
#include "kmahjongg_debug.h"
#include "kmahjonggbackground.h"
#include "kmahjongglayout.h"
#include "kmahjonggtileset.h"
#include "movelistanimation.h"
#include "prefs.h"
#include "selectionanimation.h"

GameView::GameView(GameScene * gameScene, GameData * gameData, QWidget * parent)
    : QGraphicsView(gameScene, parent)
    , m_cheatsUsed(0)
    , m_gameNumber(0)
    , m_gamePaused(false)
    , m_match(false)
    , m_gameGenerated(false)
    , m_gameData(gameData)
    , m_selectedItem(nullptr)
    , m_tilesetPath(new QString())
    , m_backgroundPath(new QString())
    , m_helpAnimation(new SelectionAnimation(this))
    , m_moveListAnimation(new MoveListAnimation(this))
    , m_demoAnimation(new DemoAnimation(this))
    , m_tiles(new KMahjonggTileset())
    , m_background(new KMahjonggBackground())
{
    // Some settings to the QGraphicsView.
    setFocusPolicy(Qt::NoFocus);
    setStyleSheet("QGraphicsView { border-style: none; }");
    setAutoFillBackground(true);

    // Read in some settings.
    m_angle = static_cast<TileViewAngle>(Prefs::angle());

    // Init HelpAnimation
    m_helpAnimation->setAnimationSpeed(ANIMATION_SPEED);
    m_helpAnimation->setRepetitions(3);

    // Init DemoAnimation
    m_demoAnimation->setAnimationSpeed(ANIMATION_SPEED);

    // Init MoveListAnimation
    m_moveListAnimation->setAnimationSpeed(ANIMATION_SPEED);

    m_selectionChangedConnect = connect(scene(), &GameScene::selectionChanged, this, &GameView::selectionChanged);

    connect(m_demoAnimation, &DemoAnimation::changeItemSelectedState, this, &GameView::changeItemSelectedState);
    connect(m_demoAnimation, &DemoAnimation::removeItem, this, &GameView::removeItem);
    connect(m_demoAnimation, &DemoAnimation::gameOver, this, &GameView::demoGameOver);

    connect(m_moveListAnimation, &MoveListAnimation::removeItem, this, &GameView::removeItem);
    connect(m_moveListAnimation, &MoveListAnimation::addItem, this, &GameView::addItemAndUpdate);
    connect(scene(), &GameScene::clearSelectedTile, this, &GameView::clearSelectedTile);
}

GameView::~GameView()
{
    delete m_helpAnimation;
    delete m_demoAnimation;
    delete m_moveListAnimation;
    delete m_background;
    delete m_tiles;
}

GameScene * GameView::scene() const
{
    return dynamic_cast<GameScene *>(QGraphicsView::scene());
}

bool GameView::checkUndoAllowed()
{
    return (m_gameData->m_allowUndo && !checkDemoAnimationActive() && !checkMoveListAnimationActive());
}

bool GameView::checkRedoAllowed()
{
    return (m_gameData->m_allowRedo && !checkDemoAnimationActive() && !checkMoveListAnimationActive());
}

long GameView::getGameNumber() const
{
    return m_gameNumber;
}

void GameView::setGameNumber(long gameNumber)
{
    m_gameNumber = gameNumber;
    setStatusText(i18n("Ready. Now it is your turn."));
}

bool GameView::undo()
{
    // Clear user selections.
    scene()->clearSelection();

    if (m_gameData->m_tileNum < m_gameData->m_maxTileNum) {
        m_gameData->clearRemovedTilePair(m_gameData->MoveListData(m_gameData->m_tileNum + 1),
                                         m_gameData->MoveListData(m_gameData->m_tileNum + 2));

        ++m_gameData->m_tileNum;
        addItemAndUpdate(m_gameData->MoveListData(m_gameData->m_tileNum));
        ++m_gameData->m_tileNum;
        addItemAndUpdate(m_gameData->MoveListData(m_gameData->m_tileNum));

        ++m_gameData->m_allowRedo;

        setStatusText(i18n("Undo operation done successfully."));

        return true;
    }

    setStatusText(i18n("What do you want to undo? You have done nothing!"));

    return false;
}

bool GameView::redo()
{
    if (m_gameData->m_allowRedo > 0) {
        m_gameData->setRemovedTilePair(m_gameData->MoveListData(m_gameData->m_tileNum),
                                       m_gameData->MoveListData(m_gameData->m_tileNum - 1));

        removeItem(m_gameData->MoveListData(m_gameData->m_tileNum));
        removeItem(m_gameData->MoveListData(m_gameData->m_tileNum));

        --m_gameData->m_allowRedo;

        return true;
    }
    return false;
}

void GameView::demoGameOver(bool won)
{
    if (won) {
        startMoveListAnimation();
    } else {
        setStatusText(i18n("Your computer has lost the game."));
        emit demoOrMoveListAnimationOver(true);
    }
}

void GameView::createNewGame(long gameNumber)
{
    setStatusText(i18n("Calculating new game..."));

    // Check any animations are running and stop them.
    checkHelpAnimationActive(true);
    checkDemoAnimationActive(true);
    checkMoveListAnimationActive(true);

    // Create a random game number, if no one was given.
    if (gameNumber == -1) {
        m_gameNumber = KRandom::random();
    } else {
        m_gameNumber = gameNumber;
    }

    m_gameData->m_allowUndo = 0;
    m_gameData->m_allowRedo = 0;
    m_gameData->random.setSeed(m_gameNumber);

    // Translate m_pGameData->Map to an array of POSITION data.  We only need to
    // do this once for each new game.
    m_gameData->generateTilePositions();

    // Now use the tile position data to generate tile dependency data.
    // We only need to do this once for each new game.
    m_gameData->generatePositionDepends();

    // TODO: This is really bad... the generatedStartPosition2() function should never fail!!!!
    // Now try to position tiles on the board, 64 tries max.
    for (short sNr = 0; sNr < 64; ++sNr) {
        if (m_gameData->generateStartPosition2()) {
            m_gameGenerated = true;

            // No cheats are used until now.
            m_cheatsUsed = 0;
            addItemsFromBoardLayout();
            populateItemNumber();

            setStatusText(i18n("Ready. Now it is your turn."));

            return;
        }
    }

    // Couldn't generate the game.
    m_gameGenerated = false;

    // Hide all generated tiles.
    foreach (GameItem * item, items()) {
        item->hide();
    }

    setStatusText(i18n("Error generating new game!"));
}

void GameView::selectionChanged()
{
    QList<GameItem *> selectedGameItems = scene()->selectedItems();

    // When no item is selected or help animation is running, there is nothing to do.
    if (selectedGameItems.size() < 1 || checkHelpAnimationActive() || checkDemoAnimationActive()) {
        return;
    }

    // If no item was already selected...
    if (m_selectedItem == nullptr) {
        // ...set the selected item.
        m_selectedItem = selectedGameItems.at(0);

        // Display the matching ones if wanted.
        if (m_match) {
            helpMatch(m_selectedItem);
        }
    } else {
        // The selected item is already there, so this is the second selected item.

        // If the same item was clicked, clear the selection and return.
        if (m_selectedItem == selectedGameItems.at(0)) {
            clearSelectedTile();
            return;
        }

        // Get both items and their positions.
        POSITION stFirstPos = m_selectedItem->getGridPos();
        POSITION stSecondPos = selectedGameItems.at(0)->getGridPos();

        // Test if the items are the same...
        if (m_gameData->isMatchingTile(stFirstPos, stSecondPos)) {
            // Update the removed tiles in GameData.
            m_gameData->setRemovedTilePair(stFirstPos, stSecondPos);

            // One tile pair is removed, so we are not allowed to redo anymore.
            m_gameData->m_allowRedo = 0;

            // Remove the items.
            removeItem(stFirstPos);
            removeItem(stSecondPos);

            // Reset the selected item variable.
            m_selectedItem = nullptr;

            // Test whether the game is over or not.
            if (m_gameData->m_tileNum == 0) {
                emit gameOver(m_gameData->m_maxTileNum, m_cheatsUsed);
            } else {
                // The game is not over, so test if there are any valid moves.
                validMovesAvailable();
            }
        } else {
            // The second tile keeps selected and becomes the first one.
            m_selectedItem = selectedGameItems.at(0);

            // Display the matching ones if wanted.
            if (m_match) {
                helpMatch(m_selectedItem);
            }
        }
    }
}

void GameView::removeItem(POSITION & stItemPos)
{
    // Adding the data to the protocoll.
    m_gameData->setMoveListData(m_gameData->m_tileNum, stItemPos);

    // Put an empty item in the data object. (data part)
    m_gameData->putTile(stItemPos.z, stItemPos.y, stItemPos.x, 0);

    // Remove the item from the scene object. (graphic part)
    scene()->removeItem(stItemPos);

    // Decrement the tilenum variable from GameData.
    m_gameData->m_tileNum = m_gameData->m_tileNum - 1;

    // If TileNum is % 2 then update the number in the status bar.
    if (!(m_gameData->m_tileNum % 2)) {
        // The item numbers changed, so we need to populate the new information.
        populateItemNumber();
    }
}

void GameView::startDemo()
{
    qCDebug(KMAHJONGG_LOG) << "Starting demo mode";

    // Create a new game with the actual game number.
    createNewGame(m_gameNumber);

    if (m_gameGenerated) {
        // Start the demo mode.
        m_demoAnimation->start(m_gameData);

        // Set the status text.
        setStatusText(i18n("Demo mode. Click mousebutton to stop."));
    }
}

void GameView::startMoveListAnimation()
{
    qCDebug(KMAHJONGG_LOG) << "Starting move list animation";

    // Stop any helping animation.
    checkHelpAnimationActive(true);

    // Stop demo animation, if anyone is running.
    checkDemoAnimationActive(true);

    m_moveListAnimation->start(m_gameData);
}

void GameView::clearSelectedTile()
{
    scene()->clearSelection();
    m_selectedItem = nullptr;
}

void GameView::changeItemSelectedState(POSITION & stItemPos, bool selected)
{
    GameItem * gameItem = scene()->getItemOnGridPos(stItemPos);

    if (gameItem != nullptr) {
        gameItem->setSelected(selected);
    }
}

void GameView::helpMove()
{
    POSITION stItem1;
    POSITION stItem2;

    // Stop a running help animation.
    checkHelpAnimationActive(true);

    if (m_gameData->findMove(stItem1, stItem2)) {
        clearSelectedTile();
        m_helpAnimation->addGameItem(scene()->getItemOnGridPos(stItem1));
        m_helpAnimation->addGameItem(scene()->getItemOnGridPos(stItem2));

        // Increase the cheat variable.
        ++m_cheatsUsed;

        m_helpAnimation->start();
    }
}

void GameView::helpMatch(GameItem const * const gameItem)
{
    int matchCount = 0;
    POSITION stGameItemPos = gameItem->getGridPos();

    // Stop a running help animation.
    checkHelpAnimationActive(true);

    // Find matching items...
    if ((matchCount = m_gameData->findAllMatchingTiles(stGameItemPos))) {
        // ...add them to the animation object...
        for (int i = 0; i < matchCount; ++i) {
            if (scene()->getItemOnGridPos(m_gameData->getFromPosTable(i)) != gameItem) {
                m_helpAnimation->addGameItem(scene()->getItemOnGridPos(
                    m_gameData->getFromPosTable(i)));
            }
        }

        // Increase the cheat variable.
        ++m_cheatsUsed;

        // ...and start the animation.
        m_helpAnimation->start();
    }
}

bool GameView::checkHelpAnimationActive(bool stop)
{
    bool active = m_helpAnimation->isActive();

    // If animation is running and it should be closed, do so.
    if (active && stop) {
        m_helpAnimation->stop();
    }

    return active;
}

bool GameView::checkMoveListAnimationActive(bool stop)
{
    bool active = m_moveListAnimation->isActive();

    // If animation is running and it should be closed, do so.
    if (active && stop) {
        m_moveListAnimation->stop();
    }

    return active;
}

bool GameView::checkDemoAnimationActive(bool stop)
{
    bool active = m_demoAnimation->isActive();

    // If animation is running and it should be closed, do so.
    if (active && stop) {
        m_demoAnimation->stop();
    }

    return active;
}

bool GameView::validMovesAvailable(bool silent)
{
    POSITION stItem1;
    POSITION stItem2;

    if (!m_gameData->findMove(stItem1, stItem2)) {
        if (!silent) {
            emit noMovesAvailable();
        }
        return false;
    }
    return true;
}

void GameView::pause(bool isPaused)
{
    if (isPaused) {
        foreach (GameItem * item, items()) {
            item->hide();
        }
    } else {
        foreach (GameItem * item, items()) {
            item->show();
        }
    }
}

bool GameView::gameGenerated()
{
    return m_gameGenerated;
}

void GameView::shuffle()
{
    if (!gameGenerated()) {
        return;
    }

    if (checkDemoAnimationActive() || checkMoveListAnimationActive()) {
        return;
    }

    // Fix bug 156022 comment 5: Redo after shuffle can cause invalid states.
    m_gameData->m_allowRedo = 0;

    m_gameData->shuffle();

    // Update the item images.
    updateItemsImages(items());

    // Cause of using the shuffle function... increase the cheat used variable.
    m_cheatsUsed += 15;

    // Populate the new item numbers.
    populateItemNumber();

    // Test if any moves are available
    validMovesAvailable();

    // Clear any tile selection done proir to the shuffle.
    clearSelectedTile();
}

void GameView::populateItemNumber()
{
    // Update the allow_undo variable, cause the item number changes.
    m_gameData->m_allowUndo = (m_gameData->m_maxTileNum != m_gameData->m_tileNum);

    emit itemNumberChanged(m_gameData->m_maxTileNum, m_gameData->m_tileNum, m_gameData->moveCount());
}

void GameView::addItemsFromBoardLayout()
{
    // The QGraphicsScene::selectionChanged() signal can be emitted when deleting or removing
    // items, so disconnect from this signal to prevent our selectionChanged() slot being
    // triggered and trying to access those items when we clear the scene.
    // The signal is reconnected at the end of the function.
    disconnect(m_selectionChangedConnect);

    // Remove all existing items.
    scene()->clear();

    // Create the items and add them to the scene.
    for (int iZ = 0; iZ < m_gameData->m_depth; ++iZ) {
        for (int iY = m_gameData->m_height - 1; iY >= 0; --iY) {
            for (int iX = m_gameData->m_width - 1; iX >= 0; --iX) {
                // Skip if no tile should be displayed on this position.
                if (!m_gameData->tilePresent(iZ, iY, iX)) {
                    continue;
                }

                POSITION stItemPos;
                stItemPos.x = iX;
                stItemPos.y = iY;
                stItemPos.z = iZ;
                stItemPos.f = (m_gameData->BoardData(iZ, iY, iX) - TILE_OFFSET);

                addItem(stItemPos, false, false, false);
            }
        }
    }

    updateItemsImages(items());
    updateItemsOrder();

    // Reconnect our selectionChanged() slot.
    m_selectionChangedConnect = connect(scene(), &GameScene::selectionChanged, this, &GameView::selectionChanged);
}

void GameView::addItem(GameItem * gameItem, bool updateImage, bool updateOrder, bool updatePosition)
{
    // Add the item to the scene.
    scene()->addItem(gameItem);

    // If TileNum is % 2 then update the number in the status bar.
    if (!(m_gameData->m_tileNum % 2)) {
        // The item numbers changed, so we need to populate the new information.
        populateItemNumber();
    }

    QList<GameItem *> gameItems;
    gameItems.append(gameItem);

    if (updateImage) {
        updateItemsImages(gameItems);
    }

    if (updatePosition) {
        // When updating the order... the position will automatically be updated after.
        if (updateOrder) {
            updateItemsOrder();
        } else {
            updateItemsPosition(gameItems);
        }
    }
}

void GameView::addItem(POSITION & stItemPos, bool updateImage, bool updateOrder, bool updatePosition)
{
    GameItem * gameItem = new GameItem(m_gameData->HighlightData(stItemPos.z, stItemPos.y, stItemPos.x));
    gameItem->setGridPos(stItemPos);
    gameItem->setFlag(QGraphicsItem::ItemIsSelectable);

    m_gameData->putTile(stItemPos.z, stItemPos.y, stItemPos.x, stItemPos.f + TILE_OFFSET);
    addItem(gameItem, updateImage, updateOrder, updatePosition);
}

void GameView::addItemAndUpdate(POSITION & stItemPos)
{
    addItem(stItemPos, true, true, true);
}

void GameView::updateItemsPosition(QList<GameItem *> gameItems)
{
    // These factor are needed for the different angles. So we simply can
    // calculate to move the items to the left or right and up or down.
    int angleXFactor = (m_angle == NE || m_angle == SE) ? -1 : 1;
    int angleYFactor = (m_angle == NW || m_angle == NE) ? -1 : 1;

    // Get half width and height of tile faces: minimum spacing = 1 pixel.
    qreal tileWidth = m_tiles->qWidth() + 0.5;
    qreal tileHeight = m_tiles->qHeight() + 0.5;

    // Get half height and width of tile-layout: ((n - 1) faces + full tile)/2.
    qreal tilesWidth = tileWidth * (m_gameData->m_width - 2) / 2
        + m_tiles->width() / 2;
    qreal tilesHeight = tileHeight * (m_gameData->m_height - 2) / 2
        + m_tiles->height() / 2;

    // Get the top-left offset required to center the items in the view.
    qreal xFrame = (width() / 2 - tilesWidth) / 2;
    qreal yFrame = (height() / 2 - tilesHeight) / 2;

    // TODO - The last /2 makes it HALF what it should be, but it gets doubled
    //        somehow before the view is painted. Why? Apparently it is because
    //        the background is painted independently by the VIEW, rather than
    //        being an item in the scene and filling the scene completely. So
    //        the whole scene is just the rectangle that contains the tiles.
    // NOTE - scene()->itemsBoundingRect() returns the correct doubled offset.

    for (int i = 0; i < gameItems.size(); ++i) {
        GameItem * gameItem = gameItems.at(i);

        // Get rasterized positions of the item.
        int x = gameItem->getGridPosX();
        int y = gameItem->getGridPosY();
        int z = gameItem->getGridPosZ();


        // Set the position of the item on the scene.
        gameItem->setPos(
            xFrame + tileWidth * x / 2
                + z * angleXFactor * (m_tiles->levelOffsetX() / 2),
            yFrame + tileHeight * y / 2
                + z * angleYFactor * (m_tiles->levelOffsetY() / 2));
    }
}

void GameView::updateItemsOrder()
{
    int zCount = 0;
    int xStart = 0;
    int xEnd = 0;
    int xCounter = 0;
    int yStart = 0;
    int yEnd = 0;
    int yCounter = 0;

    switch (m_angle) {
        case NW:
            xStart = m_gameData->m_width - 1;
            xEnd = -1;
            xCounter = -1;

            yStart = 0;
            yEnd = m_gameData->m_height;
            yCounter = 1;
            break;
        case NE:
            xStart = 0;
            xEnd = m_gameData->m_width;
            xCounter = 1;

            yStart = 0;
            yEnd = m_gameData->m_height;
            yCounter = 1;
            break;
        case SE:
            xStart = 0;
            xEnd = m_gameData->m_width;
            xCounter = 1;

            yStart = m_gameData->m_height - 1;
            yEnd = -1;
            yCounter = -1;
            break;
        case SW:
            xStart = m_gameData->m_width - 1;
            xEnd = -1;
            xCounter = -1;

            yStart = m_gameData->m_height - 1;
            yEnd = -1;
            yCounter = -1;
            break;
    }

    GameScene * gameScene = scene();

    for (int z = 0; z < m_gameData->m_depth; ++z) {
        for (int y = yStart; y != yEnd; y = y + yCounter) {
            orderLine(gameScene->getItemOnGridPos(xStart, y, z), xStart, xEnd, xCounter, y, yCounter, z, zCount);
        }
    }

    updateItemsPosition(items());
}

void GameView::orderLine(GameItem * startItem, int xStart, int xEnd, int xCounter, int y, int yCounter, int z, int & zCount)
{
    GameScene * gameScene = scene();
    GameItem * gameItem = startItem;

    for (int i = xStart; i != xEnd; i = i + xCounter) {
        if (gameItem == nullptr) {
            if ((gameItem = gameScene->getItemOnGridPos(i, y, z)) == nullptr) {
                continue;
            }
        }

        gameItem->setZValue(zCount);
        ++zCount;

        gameItem = gameScene->getItemOnGridPos(i + 2 * xCounter, y - 1 * yCounter, z);
        if (gameItem != nullptr) {
            orderLine(gameItem, i + 2 * xCounter, xEnd, xCounter, y - 1 * yCounter, yCounter, z, zCount);
            gameItem = nullptr;
        }
    }
}

bool GameView::setTilesetPath(QString const & tilesetPath)
{
    *m_tilesetPath = tilesetPath;

    if (m_tiles->loadTileset(tilesetPath)) {
        if (m_tiles->loadGraphics()) {
            resizeTileset(size());

            return true;
        }
    }

    // Tileset or graphics could not be loaded, try default
    if (m_tiles->loadDefault()) {
        if (m_tiles->loadGraphics()) {
            resizeTileset(size());

            *m_tilesetPath = m_tiles->path();
        }
    }

    return false;
}

bool GameView::setBackgroundPath(QString const & backgroundPath)
{
    qCDebug(KMAHJONGG_LOG) << "Set a new Background: " << backgroundPath;

    *m_backgroundPath = backgroundPath;

    if (m_background->load(backgroundPath, width(), height())) {
        if (m_background->loadGraphics()) {
            // Update the new background.
            updateBackground();

            return true;
        }
    }

    qCDebug(KMAHJONGG_LOG) << "Loading the background failed. Try to load the default background.";

    // Try default
    if (m_background->loadDefault()) {
        if (m_background->loadGraphics()) {
            // Update the new background.
            updateBackground();

            *m_backgroundPath = m_background->path();
        }
    }

    return false;
}

void GameView::setAngle(TileViewAngle angle)
{
    m_angle = angle;
    updateItemsImages(items());
    updateItemsOrder();
}

TileViewAngle GameView::getAngle() const
{
    return m_angle;
}

void GameView::angleSwitchCCW()
{
    switch (m_angle) {
        case SW:
            m_angle = NW;
            break;
        case NW:
            m_angle = NE;
            break;
        case NE:
            m_angle = SE;
            break;
        case SE:
            m_angle = SW;
            break;
    }

    updateItemsImages(items());
    updateItemsOrder();
}

void GameView::angleSwitchCW()
{
    switch (m_angle) {
        case SW:
            m_angle = SE;
            break;
        case SE:
            m_angle = NE;
            break;
        case NE:
            m_angle = NW;
            break;
        case NW:
            m_angle = SW;
            break;
    }

    updateItemsImages(items());
    updateItemsOrder();
}

QList<GameItem *> GameView::items() const
{
    QList<QGraphicsItem *> originalList = QGraphicsView::items();
    QList<GameItem *> tmpList;

    for (int i = 0; i < originalList.size(); ++i) {
        tmpList.append(dynamic_cast<GameItem *>(originalList.at(i)));
    }

    return tmpList;
}

void GameView::mousePressEvent(QMouseEvent * pMouseEvent)
{
    // If a move list animation is running start a new game.
    if (checkMoveListAnimationActive(true)) {
        emit demoOrMoveListAnimationOver(false);
        return;
    }

    // No mouse events when the demo mode is active.
    if (checkDemoAnimationActive(true)) {
        emit demoOrMoveListAnimationOver(false);
        return;
    }

    // If any help mode is active, ... stop it.
    checkHelpAnimationActive(true);

    // Then go on with the press event.
    QGraphicsView::mousePressEvent(pMouseEvent);
}

void GameView::resizeEvent(QResizeEvent * event)
{
    if (event->spontaneous() || m_gameData == 0) {
        return;
    }

    resizeTileset(event->size());

    m_background->sizeChanged(width(), height());
    updateBackground();

    setSceneRect(0, 0, width(), height());
}

void GameView::resizeTileset(const QSize & size)
{
    if (m_gameData == 0) {
        return;
    }

    QSize newtiles = m_tiles->preferredTileSize(size, m_gameData->m_width / 2, m_gameData->m_height / 2);

    foreach (GameItem * item, items()) {
        item->prepareForGeometryChange();
    }

    m_tiles->reloadTileset(newtiles);

    updateItemsImages(items());
    updateItemsPosition(items());
}

void GameView::updateItemsImages(QList<GameItem *> gameItems)
{
    for (int i = 0; i < gameItems.size(); ++i) {
        GameItem * gameItem = gameItems.at(i);

        QPixmap selPix;
        QPixmap unselPix;
        QPixmap facePix;

        USHORT faceId = (m_gameData->BoardData(gameItem->getGridPosZ(), gameItem->getGridPosY(), gameItem->getGridPosX()) - TILE_OFFSET);

        gameItem->setFaceId(faceId);

        facePix = m_tiles->tileface(faceId);
        selPix = m_tiles->selectedTile(m_angle);
        unselPix = m_tiles->unselectedTile(m_angle);

        // Set the background pictures to the item.
        int shadowWidth = selPix.width() - m_tiles->levelOffsetX() - facePix.width();
        int shadowHeight = selPix.height() - m_tiles->levelOffsetY() - facePix.height();

        gameItem->setAngle(m_angle, &selPix, &unselPix, shadowWidth, shadowHeight);
        gameItem->setFace(&facePix);
    }

    // Repaint the view.
    update();
}

void GameView::setStatusText(QString const & text)
{
    emit statusTextChanged(text, m_gameNumber);
}

void GameView::updateBackground()
{
    // qCDebug(KMAHJONGG_LOG) << "Update the background";
    // TODO - The background should be a scene-item? See updateItemsPosition().

    QBrush brush(m_background->getBackground());
    setBackgroundBrush(brush);
}

void GameView::setGameData(GameData * gameData)
{
    m_gameData = gameData;

    addItemsFromBoardLayout();
    populateItemNumber();
}

GameData * GameView::getGameData() const
{
    return m_gameData;
}

QString GameView::getTilesetPath() const
{
    return *m_tilesetPath;
}

QString GameView::getBackgroundPath() const
{
    return *m_backgroundPath;
}

void GameView::setMatch(bool match)
{
    m_match = match;
}

bool GameView::getMatch() const
{
    return m_match;
}
