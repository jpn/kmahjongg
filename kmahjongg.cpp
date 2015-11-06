/* kmahjongg, the classic mahjongg game for KDE project
 *
 * Copyright (C) 1997 Mathias Mueller   <in5y158@public.uni-hamburg.de>
 * Copyright (C) 2006-2007 Mauricio Piacentini   <mauricio@tabuleiro.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA. */

#include "kmahjongg.h"

#include "prefs.h"
#include "kmahjongglayoutselector.h"
#include "ui_settings.h"
#include "Editor.h"

#include <kmahjonggconfigdialog.h>
#include <QFileDialog>
#include <limits.h>

#include <QPixmapCache>
#include <QLabel>
#include <QDesktopWidget>
#include <QKeySequence>
#include <QShortcut>
#include <QStatusBar>
#include <KAboutData>
#include <QAction>
#include <KConfigDialog>
#include <QInputDialog>
#include <QMenuBar>
#include <KMessageBox>
#include <KStandardGameAction>
#include <KStandardAction>
#include <QIcon>
#include <KScoreDialog>
#include <KGameClock>

#include <kio/netaccess.h>
#include <KLocalizedString>
#include <ktoggleaction.h>
#include <kactioncollection.h>


#define ID_STATUS_TILENUMBER 1
#define ID_STATUS_MESSAGE    2
#define ID_STATUS_GAME       3


static const char *gameMagic = "kmahjongg-gamedata";
static int gameDataVersion = 1;

int is_paused = 0;

/**
 * This class implements
 *
 * longer description
 *
 * @author Mauricio Piacentini  <mauricio@tabuleiro.com> */
class Settings : public QWidget, public Ui::Settings
{
public:
    /**
     * Constructor */
    Settings(QWidget *parent)
        : QWidget(parent)
    {
        setupUi(this);
    }
};

KMahjongg::KMahjongg(QWidget *parent)
    : KXmlGuiWindow(parent)
{
    //Use up to 3MB for global application pixmap cache
    QPixmapCache::setCacheLimit(3 * 1024);

    // minimum area required to display the field
    setMinimumSize(320, 320);

    // init board widget
    bw = new BoardWidget(this);
    setCentralWidget(bw);

    // Initialize boardEditor
    boardEditor = new Editor();
    boardEditor->setModal(false);

    // Set the tileset setted in the the settings.
    boardEditor->setTilesetFromSettings();

    setupStatusBar();
    setupKAction();

    gameTimer = new KGameClock(this);

    connect(gameTimer, &KGameClock::timeChanged, this, &KMahjongg::displayTime);

    mFinished = false;
    bDemoModeActive = false;

    connect(bw, &BoardWidget::statusTextChanged, this, &KMahjongg::showStatusText);
    connect(bw, &BoardWidget::tileNumberChanged, this, &KMahjongg::showTileNumber);
    connect(bw, &BoardWidget::demoModeChanged, this, &KMahjongg::demoModeChanged);
    connect(bw, &BoardWidget::gameOver, this, &KMahjongg::gameOver);
    connect(bw, &BoardWidget::gameCalculated, this, &KMahjongg::timerReset);

    startNewGame();
}

KMahjongg::~KMahjongg()
{
    delete bw;
    delete boardEditor;
}

void KMahjongg::setupKAction()
{
    KStandardGameAction::gameNew(this, SLOT(newGame()), actionCollection());
    KStandardGameAction::load(this, SLOT(loadGame()), actionCollection());
    KStandardGameAction::save(this, SLOT(saveGame()), actionCollection());
    KStandardGameAction::quit(this, SLOT(close()), actionCollection());
    KStandardGameAction::restart(this, SLOT(restartGame()), actionCollection());

    QAction *newNumGame = actionCollection()->addAction(QStringLiteral("game_new_numeric"));
    newNumGame->setText(i18n("New Numbered Game..."));
    connect(newNumGame, &QAction::triggered, this, &KMahjongg::startNewNumeric);

    QAction *action = KStandardGameAction::hint(bw, SLOT(helpMove()), this);
    actionCollection()->addAction(action->objectName(), action);

    QAction *shuffle = actionCollection()->addAction(QStringLiteral("move_shuffle"));
    shuffle->setText(i18n("Shu&ffle"));
    shuffle->setIcon(QIcon::fromTheme(QStringLiteral("view-refresh")));
    connect(shuffle, &QAction::triggered, bw, &BoardWidget::shuffle);

    QAction *angleccw = actionCollection()->addAction(QStringLiteral("view_angleccw"));
    angleccw->setText(i18n("Rotate View Counterclockwise"));
    angleccw->setIcon(QIcon::fromTheme(QStringLiteral("object-rotate-left")));
    angleccw->setShortcut( Qt::Key_F );
    connect(angleccw, &QAction::triggered, bw, &BoardWidget::angleSwitchCCW);

    QAction *anglecw = actionCollection()->addAction(QStringLiteral("view_anglecw"));
    anglecw->setText(i18n("Rotate View Clockwise"));
    anglecw->setIcon(QIcon::fromTheme(QStringLiteral("object-rotate-right")));
    anglecw->setShortcut( Qt::Key_G );
    connect(anglecw, &QAction::triggered, bw, &BoardWidget::angleSwitchCW);

    demoAction = KStandardGameAction::demo(this, SLOT(demoMode()), actionCollection());

    KStandardGameAction::highscores(this, SLOT(showHighscores()), actionCollection());
    pauseAction = KStandardGameAction::pause(this, SLOT(pause()), actionCollection());

    // move
    undoAction = KStandardGameAction::undo(this, SLOT(undo()), actionCollection());
    redoAction = KStandardGameAction::redo(this, SLOT(redo()), actionCollection());

    // edit
    QAction *boardEdit = actionCollection()->addAction(QStringLiteral("game_board_editor"));
    boardEdit->setText(i18n("&Board Editor"));
    connect(boardEdit, &QAction::triggered, this, &KMahjongg::slotBoardEditor);

    // settings
    KStandardAction::preferences(this, SLOT(showSettings()), actionCollection());

    setupGUI(qApp->desktop()->availableGeometry().size() * 0.7);
}

void KMahjongg::setupStatusBar()
{
    gameTimerLabel = new QLabel(i18n("Time: 0:00:00"), statusBar());
    statusBar()->addWidget(gameTimerLabel);

    QFrame *timerDivider = new QFrame(statusBar());
    timerDivider->setFrameStyle(QFrame::VLine);
    statusBar()->addWidget(timerDivider);

    tilesLeftLabel = new QLabel(i18n("Removed: 0000/0000"), statusBar());
    statusBar()->addWidget(tilesLeftLabel, 1);

    QFrame *tileDivider = new QFrame(statusBar());
    tileDivider->setFrameStyle(QFrame::VLine);
    statusBar()->addWidget(tileDivider);

    gameNumLabel = new QLabel(i18n("Game: 000000000000000000000"), statusBar());
    statusBar()->addWidget(gameNumLabel);

    QFrame *gameNumDivider = new QFrame(statusBar());
    gameNumDivider->setFrameStyle(QFrame::VLine);
    statusBar()->addWidget(gameNumDivider);

    statusLabel = new QLabel(QStringLiteral("Kmahjongg"), statusBar());
    statusBar()->addWidget(statusLabel);
}

void KMahjongg::displayTime(const QString& timestring)
{
    gameTimerLabel->setText(i18n("Time: ") + timestring);
}

void KMahjongg::setDisplayedWidth()
{
    bw->setDisplayedWidth();
    bw->drawBoard();
}

void KMahjongg::startNewNumeric()
{
    bool ok;
    int s = QInputDialog::getInt(this, i18n("New Game"), i18n("Enter game number:"), 0, 0, INT_MAX, 1, &ok);

    if (ok) {
        startNewGame(s);
    }
}

void KMahjongg::undo()
{
    bw->Game->allow_redo += bw->undoMove();
    demoModeChanged(false);
}

void KMahjongg::redo()
{
    if (bw->Game->allow_redo > 0) {
        bw->Game->allow_redo--;
        bw->redoMove();
        demoModeChanged(false);
    }
}

void KMahjongg::showSettings()
{
    if (KConfigDialog::showDialog(QStringLiteral("settings"))) {
        return;
    }

    //Use the classes exposed by LibKmahjongg for our configuration dialog
    KMahjonggConfigDialog *dialog = new KMahjonggConfigDialog(this, QStringLiteral("settings"), Prefs::self());

    //The Settings class is ours
    dialog->addPage(new Settings(0), i18n("General"), QStringLiteral("games-config-options"));
    dialog->addPage(new KMahjonggLayoutSelector(0, Prefs::self()), i18n("Board Layout"), QStringLiteral("games-con"
        "fig-board"));
    dialog->addTilesetPage();
    dialog->addBackgroundPage();
#pragma message("PORT TO FRAMEWORKS")
    //dialog->setHelp(QString(),"kmahjongg");

    connect(dialog, &KMahjonggConfigDialog::settingsChanged, bw, &BoardWidget::loadSettings);
    connect(dialog, &KMahjonggConfigDialog::settingsChanged, boardEditor, &Editor::setTilesetFromSettings);
    connect(dialog, &KMahjonggConfigDialog::settingsChanged, this, &KMahjongg::setDisplayedWidth);

    dialog->show();
}

void KMahjongg::demoMode()
{
    if (bDemoModeActive) {
        bw->stopDemoMode();
    } else {
        // we assume demo mode removes tiles so we can
        // disable redo here.
        bw->Game->allow_redo = false;
        bw->startDemoMode();
    }

}

void KMahjongg::pause()
{
    if (is_paused) {
        gameTimer->resume();
    } else {
        gameTimer->pause();
    }

    is_paused = !is_paused;
    demoModeChanged(false);
    bw->pause();
}

void KMahjongg::showHighscores()
{
    KScoreDialog ksdialog(KScoreDialog::Name | KScoreDialog::Time, this);
    ksdialog.setConfigGroup(bw->getLayoutName());
    ksdialog.exec();
}

void KMahjongg::slotBoardEditor()
{
    boardEditor->setVisible(true);

    // Set the default size.
    boardEditor->setGeometry(Prefs::editorGeometry());
}

void KMahjongg::newGame()
{
    startNewGame();
}

void KMahjongg::startNewGame(int item)
{
    if (!bDemoModeActive) {
        bw->calculateNewGame(item);

        // initialise button states
        bw->Game->allow_redo = bw->Game->allow_undo = 0;

        timerReset();

        // update the initial enabled/disabled state for
        // the menu and the tool bar.
        mFinished = false;
        demoModeChanged(false);
    }
}

void KMahjongg::timerReset()
{
    // initialise the scoring system
    gameElapsedTime = 0;

    // start the game timer
    gameTimer->restart();
}

void KMahjongg::changeEvent(QEvent *event)
{
    if (event->type() == QEvent::WindowStateChange) {
        const QWindowStateChangeEvent *stateEvent = (QWindowStateChangeEvent *) event;
        const Qt::WindowStates oldMinimizedState  = stateEvent->oldState() & Qt::WindowMinimized;

        if ((isMinimized() && oldMinimizedState != Qt::WindowMinimized)
            || (!isMinimized() && oldMinimizedState == Qt::WindowMinimized)) {
            pause();
        }
    }
}

void KMahjongg::gameOver(unsigned short numRemoved,	unsigned short cheats)
{
    int time;
    int score;

    gameTimer->pause();
    long gameNum = bw->getGameNum();

    KMessageBox::information(this, i18n("You have won!"));

    mFinished = true;
    demoModeChanged(false);

    int elapsed = gameTimer->seconds();

    time = score = 0;

    // get the time in milli secs
    // subtract from 20 minutes to get bonus. if longer than 20 then ignore
    time = (60 * 20) - gameTimer->seconds();
    if (time < 0) {
        time =0;
    }
    // conv back to  secs (max bonus = 60*20 = 1200

    // points per removed tile bonus (for deragon max = 144*10 = 1440
    score += (numRemoved * 20);
    // time bonus one point per second under one hour
    score += time;
    // points per cheat penalty (max penalty = 1440 for dragon)
    score -= (cheats * 20);
    if (score < 0) {
        score = 0;
    }

    //TODO: add gameNum as a Custom KScoreDialog field?
//     theHighScores->checkHighScore(score, elapsed, gameNum, bw->getBoardName());
    KScoreDialog ksdialog(KScoreDialog::Name | KScoreDialog::Time, this);
    ksdialog.setConfigGroup(bw->getLayoutName());
    KScoreDialog::FieldInfo scoreInfo;
    scoreInfo[KScoreDialog::Score].setNum(score);
    scoreInfo[KScoreDialog::Time] = gameTimer->timeString();
    if(ksdialog.addScore(scoreInfo, KScoreDialog::AskName)) {
        ksdialog.exec();
    }

    bw->animateMoveList();

    timerReset();
}


void KMahjongg::showStatusText(const QString &msg, long board)
{
    statusLabel->setText(msg);
    QString str = i18n("Game number: %1", board);
    gameNumLabel->setText(str);
}

void KMahjongg::showTileNumber(int iMaximum, int iCurrent, int iLeft)
{
    // Hmm... seems iCurrent is the number of remaining tiles, not removed ...
    //QString szBuffer = i18n("Removed: %1/%2").arg(iCurrent).arg(iMaximum);
    QString szBuffer = i18n("Removed: %1/%2  Combinations left: %3", iMaximum-iCurrent, iMaximum,
        iLeft);
    tilesLeftLabel->setText(szBuffer);

    // Update here since undo allow is effected by demo mode
    // removal. However we only change the enabled state of the
    // items when not in demo mode
    bw->Game->allow_undo = iMaximum != iCurrent;

    // update undo menu item, if demomode is inactive
    if (!bDemoModeActive && !is_paused && !mFinished) {
        undoAction->setEnabled(bw->Game->allow_undo);
    }
}

void KMahjongg::demoModeChanged(bool bActive)
{
    bDemoModeActive = bActive;

    pauseAction->setChecked(is_paused);
    demoAction->setChecked(bActive || is_paused);

    if (is_paused) {
        stateChanged(QStringLiteral("paused"));
    } else if (mFinished) {
        stateChanged(QStringLiteral("finished"));
    } else if (bActive) {
        stateChanged(QStringLiteral("active"));
    } else {
        stateChanged(QStringLiteral("inactive"));
        undoAction->setEnabled(bw->Game->allow_undo);
        redoAction->setEnabled(bw->Game->allow_redo);
    }
}

void KMahjongg::restartGame()
{
    if (!bDemoModeActive) {
        bw->calculateNewGame(bw->getGameNum());

        // initialise button states
        bw->Game->allow_redo = bw->Game->allow_undo = 0;

        timerReset();

        // update the initial enabled/disabled state for
        // the menu and the tool bar.
        mFinished = false;
        demoModeChanged(false);

        if (is_paused) {
            pauseAction->setChecked(false);
            is_paused = false;
            bw->pause();
        }
    }
}

void KMahjongg::loadGame()
{
    QString fname;

    // Get the name of the file to load
    QUrl url = QFileDialog::getOpenFileUrl(this, i18n("Load Game" ), QUrl(), i18n("KMahjongg Game (*.kmgame)"));

    if (url.isEmpty()) {
        return;
    }

    KIO::NetAccess::download(url, fname, this);

    // open the file for reading
    QFile infile(fname);

    if (!infile.open(QIODevice::ReadOnly)) {
        KMessageBox::sorry(this, i18n("Could not read from file. Aborting."));
        return;
    }

    QDataStream in(&infile);

    // verify the magic
    QString magic;
    in >> magic;

    if (QString::compare(magic, gameMagic, Qt::CaseSensitive) != 0) {
        KMessageBox::sorry(this, i18n("File is not a KMahjongg game."));
        infile.close();

        return;
    }

    // Read the version
    qint32 version;
    in >> version;

    if (version == gameDataVersion) {
        in.setVersion(QDataStream::Qt_4_0);
    } else {
        KMessageBox::sorry(this, i18n("File format not recognized."));
        infile.close();

        return;
    }

    QString theTilesName;
    QString theBackgroundName;
    QString theBoardLayoutName;
    in >> theTilesName;
    bw->loadTileset(theTilesName);
    in >> theBackgroundName;
    bw->loadBackground(theBackgroundName, false);
    in >> theBoardLayoutName;

    //GameTime
    uint seconds;
    in >> seconds;
    gameTimer->setTime(seconds);

    delete bw->Game;
    bw->loadBoardLayout(theBoardLayoutName);
    bw->Game = new GameData(bw->theBoardLayout.board());
    bw->Game->loadFromStream(in);

    infile.close();

    KIO::NetAccess::removeTempFile(fname);

    // refresh the board
    bw->gameLoaded();

    mFinished = false;
    demoModeChanged(false);
}

void KMahjongg::saveGame()
{
    //Pause timer
    gameTimer->pause();

    // Get the name of the file to save
    QUrl url = QFileDialog::getSaveFileUrl(this, i18n("Save Game"), QUrl(), i18n("KMahjongg Game (*.kmgame)"));

    if (url.isEmpty()) {
        gameTimer->resume();

        return;
    }

    if (!url.isLocalFile()) {
        KMessageBox::sorry(this, i18n("Only saving to local files currently supported."));
        gameTimer->resume();

        return;
    }

    QFile outfile(url.path());

    if (!outfile.open(QIODevice::WriteOnly)) {
        KMessageBox::sorry(this, i18n("Could not write saved game."));
        gameTimer->resume();

        return;
    }

    QDataStream out(&outfile);

    // Write a header with a "magic number" and a version
    out << QString(gameMagic);
    out << (qint32) gameDataVersion;
    out.setVersion(QDataStream::Qt_4_0);

    out << bw->theTiles.path();
    out << bw->theBackground.path();
    out << bw->theBoardLayout.path();

    //GameTime
    out << gameTimer->seconds();
    // Write the Game data
    bw->Game->saveToStream(out);

    outfile.close();
    gameTimer->resume();
}



