/**********************************************************************

	--- Qt Architect generated file ---

	File: PrefsDlgData.cpp
	Last generated: Fri Apr 2 13:23:56 1999

	DO NOT EDIT!!!  This file will be automatically
	regenerated by qtarch.  All changes will be lost.

 *********************************************************************/

#include "PrefsDlg.h"
#include "klocale.h"
                       
#include "Preferences.h"
#include "PrefsDlg.moc"

#define Inherited QDialog

#include <qbttngrp.h>
#include <qpushbt.h>
#include <qchkbox.h>
#include <qradiobt.h> 


PrefsDlgData::PrefsDlgData
(
	QWidget* parent,
	const char* name
)
	:
	Inherited( parent, name, TRUE, 0 )
{
	QButtonGroup* BackgroundGroup;
	BackgroundGroup = new QButtonGroup( this, "BackgroundGroup" );
	BackgroundGroup->setGeometry( 198, 66, 187, 56 );
//	BackgroundGroup->setFrameStyle( 49 );
	BackgroundGroup->setTitle( i18n("Background") );

	QButtonGroup* TilesGroup;
	TilesGroup = new QButtonGroup( this, "TilesGroup" );
	TilesGroup->setGeometry( 198, 5, 187, 56 );
	TilesGroup->setTitle( i18n("Tiles") );

	QButtonGroup* KmahjonggGroup;
	KmahjonggGroup = new QButtonGroup( this, "KmahjonggGroup" );
	KmahjonggGroup->setGeometry( 5, 5, 188, 117 );
	KmahjonggGroup->setTitle( i18n("Kmahjong") );

	dlgTileBg = new QRadioButton( BackgroundGroup, "tileRB" );
	dlgTileBg->setGeometry( 5, 16, 177, 16 );
	dlgTileBg->setText( i18n("Tile") );

	dlgScaleBg = new QRadioButton( BackgroundGroup, "scaleRB" );
	dlgScaleBg->setGeometry( 5, 38, 177, 16 );
	dlgScaleBg->setText( i18n("Scale") );
	dlgScaleBg->setChecked( TRUE );

	dlgShowShadows = new QCheckBox( TilesGroup, "shadowsCB" );
	dlgShowShadows->setGeometry( 5, 16, 177, 17 );
	dlgShowShadows->setText( i18n("Draw shadows") );
	dlgShowShadows->setChecked( TRUE );

	dlgShowRemoved = new QCheckBox( KmahjonggGroup, "removedCB" );
	dlgShowRemoved->setGeometry( 5, 19, 178, 28 );
	dlgShowRemoved->setText( i18n("Show removed tiles") );
	dlgShowRemoved->setChecked( TRUE );

	dlgSavePrefs = new QCheckBox( KmahjonggGroup, "saveCB" );
	dlgSavePrefs->setGeometry( 5, 52, 178, 27 );
	dlgSavePrefs->setText( i18n("Save Preferences on exit") );
	dlgSavePrefs->setChecked( TRUE );

	dlgShowStatus = new QCheckBox( KmahjonggGroup, "stautsCB" );
	dlgShowStatus->setGeometry( 5, 84, 178, 28 );
	dlgShowStatus->setText( i18n("Show status bar") );
	dlgShowStatus->setChecked( TRUE );

	QPushButton* applyBtn;
	applyBtn = new QPushButton( this, "applyBtn" );
	applyBtn->setGeometry( 133, 127, 124, 28 );
	connect( applyBtn, SIGNAL(clicked()), SLOT(acceptClicked()) );
	applyBtn->setText( i18n("Apply") );

	QPushButton* OkBtn;
	OkBtn = new QPushButton( this, "OkBtn" );
	OkBtn->setGeometry( 5, 127, 123, 28 );
	connect( OkBtn, SIGNAL(clicked()), SLOT(okClicked()) );
	OkBtn->setText( i18n("OK") );

	QPushButton* cancelBtn;
	cancelBtn = new QPushButton( this, "cancelBtn" );
	cancelBtn->setGeometry( 262, 127, 123, 28 );
	connect( cancelBtn, SIGNAL(clicked()), SLOT(reject()) );
	cancelBtn->setText( i18n("Cancel") );

	dlgMiniTiles = new QCheckBox( TilesGroup, "miniCB" );
	dlgMiniTiles->setGeometry( 5, 34, 177, 17 );
	dlgMiniTiles->setText( i18n("Use mini-tiles") );

	BackgroundGroup->insert( dlgTileBg );
	BackgroundGroup->insert( dlgScaleBg );

	TilesGroup->insert( dlgShowShadows );
	TilesGroup->insert( dlgMiniTiles );

	KmahjonggGroup->insert( dlgShowRemoved );
	KmahjonggGroup->insert( dlgSavePrefs );
	KmahjonggGroup->insert( dlgShowStatus );



	resize( 390,160 );


	setMinimumSize( 0, 0 );
	setMaximumSize( 32767, 32767 );
}


PrefsDlgData::~PrefsDlgData()
{
}
void PrefsDlgData::acceptClicked()
{
}
void PrefsDlgData::okClicked()
{
}

#undef Inherited
#define Inherited PrefsDlgData

PrefsDlg::PrefsDlg
(
	QWidget* parent,
	const char* name
)
	:
	Inherited( parent, name )
{
}

void PrefsDlg::initialise(void) {


	// initialise the checkboxes
	dlgShowStatus->setChecked((oStatus = preferences.showStatus()));
	dlgShowShadows->setChecked((oShadows = preferences.showShadows()));
	dlgMiniTiles->setChecked((oMini= preferences.miniTiles()));
	dlgSavePrefs->setChecked((oSave = preferences.autosave()));
	dlgShowRemoved->setChecked((oRemoved = preferences.showRemoved()));
	
	// initialise the radoi buttons
	dlgScaleBg->setChecked((oScale = preferences.scaleMode()));
	dlgTileBg->setChecked(!preferences.scaleMode());

}

void PrefsDlg::updatePreferences(void) {
	int redrawRequired = 0;
	int tmp;

	// status bar configuration test	
	if ((tmp=dlgShowStatus->isChecked()) != oStatus) {
		preferences.setStatus(tmp);	
		oStatus = tmp;
		statusBar(tmp);
	}

	// show shadows configuration test	
	if ((tmp=dlgShowShadows->isChecked()) != oShadows) {
		preferences.setShadows(tmp);	
		oShadows = tmp;
		redrawRequired++;
	}

	if ((tmp=dlgShowRemoved->isChecked()) != oRemoved) {
		preferences.setRemoved(tmp);	
		oRemoved = tmp;
		showRemovedChanged();
		
	}

	if ((tmp=dlgScaleBg->isChecked()) != oScale) {
		preferences.setScale(tmp);	
		backgroundModeChanged();
		oScale = tmp;
		redrawRequired++;
	}
	
	// change to the use mini tiles options
	if ((tmp=dlgMiniTiles->isChecked()) != oMini) {
		preferences.setMiniTiles(tmp);	
		oMini= tmp;
		tileSizeChanged();
	}
	
	if ((tmp=dlgSavePrefs->isChecked()) != oSave ) {
		preferences.setAutosave(tmp);	
		oSave = tmp;
	}
	if (redrawRequired != 0) {
		boardRedraw(true);
	}
	
}

// Slots for button presses

void PrefsDlg::acceptClicked() {
	updatePreferences();	
}

void PrefsDlg::okClicked() {
	updatePreferences();
	accept();
}


PrefsDlg::~PrefsDlg()
{
}