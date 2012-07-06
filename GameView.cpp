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

#include "GameView.h"
#include "GameData.h"
#include "GameScene.h"
#include "GameItem.h"
#include "SelectionAnimation.h"
#include "MoveListAnimation.h"
#include "DemoAnimation.h"
#include "kmahjongglayout.h"
#include "kmahjonggtileset.h"
#include "kmahjonggbackground.h"
#include "prefs.h"

#include <KLocale>
#include <KRandom>
#include <KDebug>
#include <KMessageBox>

#include <QResizeEvent>
#include <QMouseEvent>


GameView::GameView(GameScene *pGameScene, QWidget *pParent)
    : QGraphicsView(pGameScene, pParent),
    m_pGameData(NULL),
    m_pBoardLayoutPath(new QString()),
    m_pBackgroundPath(new QString()),
    m_pTilesetPath(new QString()),
    m_pBoardLayout(new KMahjonggLayout()),
    m_pBackground(new KMahjonggBackground()),
    m_pTiles(new KMahjonggTileset()),
    m_pSelectedItem(NULL),
    m_pHelpAnimation(new SelectionAnimation(this)),
    m_pDemoAnimation(new DemoAnimation(this)),
    m_pMoveListAnimation(new MoveListAnimation(this)),
    m_bMatch(false)
{
    // Some settings to the QGraphicsView.
    setFocusPolicy(Qt::NoFocus);
    setStyleSheet( "QGraphicsView { border-style: none; }" );

    // Read in some settings.
    m_angle = (TileViewAngle) Prefs::angle();

    // Init HelpAnimation
    m_pHelpAnimation->setAnimationSpeed(ANIMATION_SPEED);
    m_pHelpAnimation->setRepetitions(2);

    // Init DemoAnimation
    m_pDemoAnimation->setAnimationSpeed(ANIMATION_SPEED);

    // Init MoveListAnimation
    m_pMoveListAnimation->setAnimationSpeed(ANIMATION_SPEED);

    // Connections
    connect(scene(), SIGNAL(selectionChanged()), this, SLOT(selectionChanged()));

    connect(m_pDemoAnimation, SIGNAL(changeItemSelectedState(POSITION &, bool)), this,
        SLOT(changeItemSelectedState(POSITION &, bool)));
    connect(m_pDemoAnimation, SIGNAL(removeItem(POSITION &)), this, SLOT(removeItem(POSITION &)));
    connect(m_pDemoAnimation, SIGNAL(gameOver(bool)), this, SLOT(demoGameOver(bool)));

    connect(m_pMoveListAnimation, SIGNAL(removeItem(POSITION &)), this,
        SLOT(removeItem(POSITION &)));
    connect(m_pMoveListAnimation, SIGNAL(addItem(POSITION &)), this,
        SLOT(addItemAndUpdate(POSITION &)));
}

GameView::~GameView()
{
    delete m_pHelpAnimation;
    delete m_pDemoAnimation;
    delete m_pMoveListAnimation;
    delete m_pBackground;
    delete m_pTiles;
    delete m_pGameData;
}

GameScene * GameView::scene() const
{
    return dynamic_cast<GameScene *>(QGraphicsView::scene());
}

void GameView::demoGameOver(bool bWon)
{
    if (bWon) {
        startMoveListAnimation();
    } else {
        setStatusText(i18n("Your computer has lost the game."));
    }
}

void GameView::createNewGame(int iGameNumber)
{
    setStatusText(i18n("Calculating new game..."));

    // Check any animations are running and stop them.
    checkHelpAnimationActive(true);
    checkDemoAnimationActive(true);

    // Create a random game number, if no one was given.
    if (iGameNumber == -1) {
        m_lGameNumber = KRandom::random();
    } else {
        m_lGameNumber = iGameNumber;
    }

    m_pGameData->random.setSeed(m_lGameNumber);

    // Translate m_pGameData->Map to an array of POSITION data.  We only need to
    // do this once for each new game.
    m_pGameData->generateTilePositions();

    // Now use the tile position data to generate tile dependency data.
    // We only need to do this once for each new game.
    m_pGameData->generatePositionDepends();

    // TODO: This is really bad... the generatedStartPosition2-function should never fail!!!!
    // Now try to position tiles on the board, 64 tries max.
    for (short sNr = 0; sNr < 64; sNr++) {
        if (m_pGameData->generateStartPosition2()) {
            // No cheats are used until now.
            m_usCheatsUsed = 0;
            addItemsFromBoardLayout();
            populateItemNumber();

            setStatusText(i18n("Ready. Now it is your turn."));

            return;
        }
    }

    setVisible(!isVisible());
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
    if (m_pSelectedItem == NULL) {
        // ...set the selected item.
        m_pSelectedItem = selectedGameItems.at(0);

        // Display the matching ones if wanted.
        if (m_bMatch) {
            helpMatch(m_pSelectedItem);
        }
    } else {
        // The selected item is already there, so this is the second selected item.

        // Get both items and their positions.
        POSITION stFirstPos = getPositionFromItem(m_pSelectedItem);
        POSITION stSecondPos = getPositionFromItem(selectedGameItems.at(0));

        // Test if the items are the same...
        if (m_pGameData->isMatchingTile(stFirstPos, stSecondPos)) {
            // Update the removed tiles in GameData.
            m_pGameData->setRemovedTilePair(stFirstPos, stSecondPos);

            // Remove the items.
            removeItem(stFirstPos);
            removeItem(stSecondPos);

            // Reset the selected item variable.
            m_pSelectedItem = NULL;

            // Test whether the game is over or not.
            if (m_pGameData->TileNum == 0) {
                emit gameOver(m_pGameData->MaxTileNum, m_usCheatsUsed);
            } else {
                // The game is not over, so test if there are any valid moves.
                validMovesAvailable();
            }
        } else {
            // The second tile keeps selected and becomes the first one.
            m_pSelectedItem = selectedGameItems.at(0);

            // Display the matching ones if wanted.
            if (m_bMatch) {
                helpMatch(m_pSelectedItem);
            }
        }
    }
}

void GameView::removeItem(POSITION & stItemPos)
{
    // Adding the data to the protocoll.
    m_pGameData->setMoveListData(m_pGameData->TileNum, stItemPos);

    // Put an empty item in the data object. (data part)
    m_pGameData->putTile(stItemPos.e, stItemPos.y, stItemPos.x, 0);

    // Remove the item from the scene object. (graphic part)
    scene()->removeItem(stItemPos);

    // Decrement the tilenum variable from GameData.
    m_pGameData->TileNum = m_pGameData->TileNum - 1;

    // If TileNum is % 2 then update the number in the status bar.
    if (!(m_pGameData->TileNum % 2)) {
        // The item numbers changed, so we need to populate the new informations.
        populateItemNumber();
    }
}

void GameView::startDemo()
{
    kDebug() << "Starting demo mode";

    // Stop any helping animation.
    checkHelpAnimationActive(true);

    // Stop demo animation, if anyone is running.
    checkDemoAnimationActive(true);

    // Create a new game with the actual game number.
    createNewGame(m_lGameNumber);

    // Start the demo mode.
    m_pDemoAnimation->start(m_pGameData);
}

void GameView::startMoveListAnimation()
{
    kDebug() << "Starting move list animation";

    // Stop any helping animation.
    checkHelpAnimationActive(true);

    // Stop demo animation, if anyone is running.
    checkDemoAnimationActive(true);

    m_pMoveListAnimation->start(m_pGameData);
}

void GameView::changeItemSelectedState(POSITION & stItemPos, bool bSelected)
{
    getItemFromPosition(stItemPos)->setSelected(bSelected);
}

void GameView::helpMove()
{
    POSITION stItem1;
    POSITION stItem2;

    // Stop a running help animation.
    checkHelpAnimationActive(true);

    if (m_pGameData->findMove(stItem1, stItem2)) {
        m_pHelpAnimation->clearGameItems();
        m_pHelpAnimation->addGameItem(getItemFromPosition(stItem1));
        m_pHelpAnimation->addGameItem(getItemFromPosition(stItem2));

        // Increase the cheat variable.
        m_usCheatsUsed++;

        m_pHelpAnimation->start();
    }
}

void GameView::helpMatch(GameItem * pGameItem)
{
    int iMatchCount = 0;
    POSITION stGameItemPos = getPositionFromItem(pGameItem);

    // Stop a running help animation.
    checkHelpAnimationActive(true);

    // Find matching items...
    if (iMatchCount = m_pGameData->findAllMatchingTiles(stGameItemPos)) {
        m_pHelpAnimation->clearGameItems();

        // ...add them to the animation object...
        for (int i = 0; i < iMatchCount; i++) {
            m_pHelpAnimation->addGameItem(getItemFromPosition(m_pGameData->getFromPosTable(i)));
        }

        // Increase the cheat variable.
        m_usCheatsUsed++;

        // ...and start the animation.
        m_pHelpAnimation->start();
    }
}

bool GameView::checkHelpAnimationActive(bool bStop)
{
    bool bActive = m_pHelpAnimation->isActive();

    // If animation is running and it should be closed, do so.
    if (bActive && bStop) {
        m_pHelpAnimation->stop();
    }

    return bActive;
}

bool GameView::checkDemoAnimationActive(bool bStop)
{
    bool bActive = m_pDemoAnimation->isActive();

    // If animation is running and it should be closed, do so.
    if (bActive && bStop) {
        m_pDemoAnimation->stop();
    }

    return bActive;
}

bool GameView::validMovesAvailable(bool bSilent)
{
    POSITION stItem1;
    POSITION stItem2;

    if (!m_pGameData->findMove(stItem1, stItem2)) {
        if (!bSilent) {
            KMessageBox::information(this, i18n("Game over: You have no moves left."));
        }

        return false;
    }

    return true;
}

void GameView::shuffle()
{
    m_pGameData->shuffle();

    // Update the item images.
    updateItemsImages(items());

    // Cause of using the shuffle function... increase the cheat used variable.
    m_usCheatsUsed += 15;

    // Populate the new item numbers.
    populateItemNumber();

    // Test if any moves are available
    validMovesAvailable();
}

void GameView::populateItemNumber()
{
    emit itemNumberChanged(m_pGameData->MaxTileNum, m_pGameData->TileNum, m_pGameData->moveCount());
}

GameItem * GameView::getItemFromPosition(POSITION stGamePos)
{
    return scene()->getItemOnPosition(stGamePos.x, stGamePos.y, stGamePos.e);
}

POSITION GameView::getPositionFromItem(GameItem * pGameItem)
{
    POSITION stPos;

    stPos.e = pGameItem->getZPosition();
    stPos.x = pGameItem->getXPosition();
    stPos.y = pGameItem->getYPosition();
    stPos.f = m_pGameData->BoardData(stPos.e, stPos.y, stPos.x) - TILE_OFFSET;

    return stPos;
}

void GameView::addItemsFromBoardLayout()
{
    GameScene *pGameScene = scene();

    // Remove all existing items.
    pGameScene->clear();

    // Create the items and add them to the scene.
    for (int iZ = 0; iZ < m_pGameData->m_depth; iZ++) {
        for (int iY = m_pGameData->m_height - 1; iY >= 0; iY--) {
            for (int iX = m_pGameData->m_width - 1; iX >= 0; iX--) {

                // Skip if no tile should be displayed on this position.
                if (!m_pGameData->tilePresent(iZ, iY, iX)) {
                    continue;
                }

                bool bSelected = false;

                if (m_pGameData->HighlightData(iZ, iY, iX)) {
                    bSelected = true;
                }

                GameItem *item = new GameItem(bSelected);
                item->setPosition(iX, iY, iZ);
                item->setFlag(QGraphicsItem::ItemIsSelectable);
                addItem(item);
            }
        }
    }

    updateItemsImages(items());
    updateItemsOrder();
}

void GameView::addItem(GameItem * pGameItem, bool bUpdateImage, bool bUpdateOrder,
    bool bUpdatePosition)
{
    // Add the itme to the scene.
    scene()->addItem(pGameItem);

    // If TileNum is % 2 then update the number in the status bar.
    if (!(m_pGameData->TileNum % 2)) {
        // The item numbers changed, so we need to populate the new informations.
        populateItemNumber();
    }

    QList<GameItem *> gameItems;
    gameItems.append(pGameItem);

    if (bUpdateImage) {
        updateItemsImages(gameItems);
    }

    if (bUpdatePosition) {
        // When updating the order... the position will automatically be updated after.
        if (bUpdateOrder) {
            updateItemsOrder();
        } else {
            updateItemsPosition(gameItems);
        }
    }
}

void GameView::addItem(POSITION & stItemPos, bool bUpdateImage, bool bUpdateOrder,
    bool bUpdatePosition)
{
    GameItem * pGameItem = new GameItem(m_pGameData->HighlightData(stItemPos.e, stItemPos.y,
        stItemPos.x));
    pGameItem->setPosition(stItemPos.x, stItemPos.y, stItemPos.e);
    pGameItem->setFlag(QGraphicsItem::ItemIsSelectable);

    m_pGameData->putTile(stItemPos.e, stItemPos.y, stItemPos.x, stItemPos.f);
    addItem(pGameItem, bUpdateImage, bUpdateOrder, bUpdatePosition);
}

void GameView::addItemAndUpdate(POSITION & stItemPos)
{
    addItem(stItemPos, true, true, true);
}

void GameView::updateItemsPosition(QList<GameItem *> gameItems)
{
    // These factor are needed for the different angles. So we simply can calculate to move the
    // items to the left or right (eg up or down).
    int iAngleXFactor = (m_angle == NE || m_angle == SE) ? -1 : 1;
    int iAngleYFactor = (m_angle == NW || m_angle == NE) ? -1 : 1;

    // Get the item width and height.
    int iTileWidth = m_pTiles->qWidth();
    int iTileHeight = m_pTiles->qHeight();

    // Get the items height and width.
    int iTilesWidth = iTileWidth * (m_pGameData->m_width / 2);
    int iTilesHeight = iTileHeight * (m_pGameData->m_height / 2);

    // The frame of the window to center the items in the view.
    int iXFrame = (width() / 2 - iTilesWidth) / 2 + (m_pTiles->levelOffsetX());
    int iYFrame = (height() / 2 - iTilesHeight) / 2 + (m_pTiles->levelOffsetY());

    for (int iI = 0; iI < gameItems.size(); iI++) {
        GameItem *pGameItem = gameItems.at(iI);

        // Get rasterized positions of the item.
        int iX = pGameItem->getXPosition() - 1;
        int iY = pGameItem->getYPosition() - 1;
        int iZ = pGameItem->getZPosition();

        // Set the position of the item on the view.
        pGameItem->setPos(iTileWidth / 2 * iX + iXFrame + iZ * iAngleXFactor *
            (m_pTiles->levelOffsetX() / 2), iTileHeight / 2 * iY + iYFrame + iZ * iAngleYFactor *
            (m_pTiles->levelOffsetY() / 2));
    }
}

void GameView::updateItemsOrder()
{
    int iZCount = 0;
    int iXStart;
    int iXEnd;
    int iXCounter;
    int iYStart;
    int iYEnd;
    int iYCounter;

    switch (m_angle) {
    case NW:
        kDebug() << "NW";
        iXStart = m_pGameData->m_width - 1;
        iXEnd = -1;
        iXCounter = -1;

        iYStart = 0;
        iYEnd = m_pGameData->m_height;
        iYCounter = 1;
        break;
    case NE:
        kDebug() << "NE";
        iXStart = 0;
        iXEnd = m_pGameData->m_width;
        iXCounter = 1;

        iYStart = 0;
        iYEnd = m_pGameData->m_height;
        iYCounter = 1;
        break;
    case SE:
        kDebug() << "SE";
        iXStart = 0;
        iXEnd = m_pGameData->m_width;
        iXCounter = 1;

        iYStart = m_pGameData->m_height - 1;
        iYEnd = -1;
        iYCounter = -1;
        break;
    case SW:
        kDebug() << "SW";
        iXStart = m_pGameData->m_width - 1;
        iXEnd = -1;
        iXCounter = -1;

        iYStart = m_pGameData->m_height - 1;
        iYEnd = -1;
        iYCounter = -1;
        break;
    }

    GameScene *pGameScene = scene();

    for (int iZ = 0; iZ < m_pGameData->m_depth; iZ++) {
        for (int iY = iYStart; iY != iYEnd; iY = iY + iYCounter) {
            orderLine(pGameScene->getItemOnPosition(iXStart, iY, iZ), iXStart, iXEnd, iXCounter, iY,
                iYCounter, iZ, iZCount);
        }
    }

    updateItemsPosition(items());
}

void GameView::orderLine(GameItem * pStartItem, int iXStart, int iXEnd, int iXCounter, int iY,
    int iYCounter, int iZ, int &iZCount)
{
    GameScene * pGameScene = scene();
    GameItem * pGameItem = pStartItem;

    for (int i = iXStart; i != iXEnd; i = i + iXCounter) {
        if (pGameItem == NULL) {
            if ((pGameItem = pGameScene->getItemOnPosition(i, iY, iZ)) == NULL) {
                continue;
            }
        }

        pGameItem->setZValue(iZCount);
        iZCount++;

        pGameItem = pGameScene->getItemOnPosition(i + 2 * iXCounter, iY - 1 * iYCounter, iZ);
        if (pGameItem != NULL) {
            orderLine(pGameItem, i + 2 * iXCounter, iXEnd, iXCounter, iY - 1 * iYCounter, iYCounter,
                iZ, iZCount);
            pGameItem = NULL;
        }
    }
}

bool GameView::loadBoardLayoutFromPath()
{
    if (m_pBoardLayout->load(*m_pBoardLayoutPath)) {
        return true;
    } else {
        if (m_pBoardLayout->loadDefault()) {
            return false;
        } else {
            return false;
        }
    }
}

bool GameView::setBoardLayoutPath(QString const &rBoardLayoutPath)
{
    *m_pBoardLayoutPath = rBoardLayoutPath;

    kDebug() << *m_pBoardLayoutPath;

    // Load the new set board layout.
    loadBoardLayoutFromPath();

    // We need to create a new GameData object.
    GameData *pOldGameData = m_pGameData;
    m_pGameData = new GameData(m_pBoardLayout->board());

    // New game data object so set, so delete the old one.
    delete pOldGameData;

    return true;
}

bool GameView::setTilesetPath(QString const &rTilesetPath)
{
    *m_pTilesetPath = rTilesetPath;

    if (m_pTiles->loadTileset(rTilesetPath)) {
        if (m_pTiles->loadGraphics()) {
            resizeTileset(size());

            return true;
        }
    }

    // Tileset or graphics could not be loaded, try default
    if (m_pTiles->loadDefault()) {
        if (m_pTiles->loadGraphics()) {
            resizeTileset(size());
        }
    }

    return false;
}

bool GameView::setBackgroundPath(QString const &rBackgroundPath)
{
    *m_pBackgroundPath = rBackgroundPath;

    if (m_pBackground->load(rBackgroundPath, width(), height())) {
        if (m_pBackground->loadGraphics()) {
            // Update the new background.
            updateBackground();

            return true;
        }
    }

    // Try default
    if (m_pBackground->loadDefault()) {
        if (m_pBackground->loadGraphics()) {
            // Update the new background.
            updateBackground();
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

    for (int i = 0; i < originalList.size(); i++) {
        tmpList.append(dynamic_cast<GameItem *>(originalList.at(i)));
    }

    return tmpList;
}

void GameView::mousePressEvent(QMouseEvent * pMouseEvent)
{
    // No mouse events when the demo mode is active.
    if (checkDemoAnimationActive()) {
        return;
    }

    // If any help mode is active, ... stop it.
    checkHelpAnimationActive(true);

    // Then go on with the press event.
    QGraphicsView::mousePressEvent(pMouseEvent);
}

void GameView::resizeEvent(QResizeEvent *pEvent)
{
    if (pEvent->spontaneous() || m_pGameData == 0) {
        return;
    }

    resizeTileset(pEvent->size());
    m_pBackground->sizeChanged(m_pGameData->m_width / 2, m_pGameData->m_height / 2);

    setSceneRect(0, 0, width(), height());
}

void GameView::resizeTileset(QSize const &rSize)
{
    if (m_pGameData == 0) {
        return;
    }

    QSize newtiles = m_pTiles->preferredTileSize(rSize, m_pGameData->m_width / 2,
        m_pGameData->m_height / 2);

    m_pTiles->reloadTileset(newtiles);

    updateItemsImages(items());
    updateItemsPosition(items());
}

void GameView::updateItemsImages(QList<GameItem *> gameItems)
{
    for (int i = 0; i < gameItems.size(); i++) {
        GameItem *pGameItem = gameItems.at(i);

        QPixmap selPix;
        QPixmap unselPix;
        QPixmap facePix;

        facePix = m_pTiles->tileface(m_pGameData->BoardData(pGameItem->getZPosition(),
            pGameItem->getYPosition(), pGameItem->getXPosition()) - TILE_OFFSET);
        selPix = m_pTiles->selectedTile(m_angle);
        unselPix = m_pTiles->unselectedTile(m_angle);

        // Set the background pictures to the item.
        int iShadowWidth = selPix.width() - m_pTiles->levelOffsetX() - facePix.width();
        int iShadowHeight = selPix.height() - m_pTiles->levelOffsetY() - facePix.height();

        pGameItem->setAngle(m_angle, &selPix, &unselPix, iShadowWidth, iShadowHeight);
        pGameItem->setFace(&facePix);
    }

    // Repaint the view.
    update();
}

void GameView::setStatusText(QString const &rText)
{
    emit statusTextChanged(rText, m_lGameNumber);
}

void GameView::updateBackground()
{
    QPalette palette;
    palette.setBrush(backgroundRole(), m_pBackground->getBackground());
    setPalette(palette);
    setAutoFillBackground(true);
}

void GameView::setGameData(GameData *pGameData)
{
    m_pGameData = pGameData;
}

GameData * GameView::getGameData()
{
    return m_pGameData;
}

QString GameView::getTilesetPath()
{
    return *m_pTilesetPath;
}

QString GameView::getBackgroundPath()
{
    return *m_pBackgroundPath;
}

QString GameView::getBoardLayoutPath()
{
    return *m_pBoardLayoutPath;
}

void GameView::setMatch(bool bMatch)
{
    m_bMatch = bMatch;
}

bool GameView::getMatch() const
{
    return m_bMatch;
}
