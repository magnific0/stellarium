/*
 * Stellarium Quasars Plug-in GUI
 *
 * Copyright (C) 2012 Alexander Wolf
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Suite 500, Boston, MA  02110-1335, USA.
*/

#include <QDebug>
#include <QTimer>
#include <QDateTime>
#include <QUrl>
#include <QFileDialog>

#include "StelApp.hpp"
#include "ui_quasarsDialog.h"
#include "QuasarsDialog.hpp"
#include "Quasars.hpp"
#include "StelModuleMgr.hpp"
#include "StelObjectMgr.hpp"
#include "StelMovementMgr.hpp"
#include "StelStyle.hpp"
#include "StelGui.hpp"
#include "StelMainView.hpp"
#include "StelFileMgr.hpp"
#include "StelTranslator.hpp"

QuasarsDialog::QuasarsDialog()
	: qsr(NULL)
	, updateTimer(NULL)
{
	ui = new Ui_quasarsDialog;
	dialogName = "Quasars";
}

QuasarsDialog::~QuasarsDialog()
{
	if (updateTimer)
	{
		updateTimer->stop();
		delete updateTimer;
		updateTimer = NULL;
	}
	delete ui;
}

void QuasarsDialog::retranslate()
{
	if (dialog)
	{
		ui->retranslateUi(dialog);
		refreshUpdateValues();
		setAboutHtml();
	}
}

// Initialize the dialog widgets and connect the signals/slots
void QuasarsDialog::createDialogContent()
{
	qsr = GETSTELMODULE(Quasars);
	ui->setupUi(dialog);
	ui->tabs->setCurrentIndex(0);	
	connect(&StelApp::getInstance(), SIGNAL(languageChanged()),
		this, SLOT(retranslate()));

	// Settings tab / updates group
	ui->displayModeCheckBox->setChecked(qsr->getDisplayMode());
	connect(ui->displayModeCheckBox, SIGNAL(stateChanged(int)), this, SLOT(setDistributionEnabled(int)));
	ui->displayAtStartupCheckBox->setChecked(qsr->getEnableAtStartup());
	connect(ui->displayAtStartupCheckBox, SIGNAL(stateChanged(int)), this, SLOT(setDisplayAtStartupEnabled(int)));
	ui->displayShowQuasarsButton->setChecked(qsr->getFlagShowQuasarsButton());
	connect(ui->displayShowQuasarsButton, SIGNAL(stateChanged(int)), this, SLOT(setDisplayShowQuasarsButton(int)));
	connect(ui->internetUpdatesCheckbox, SIGNAL(stateChanged(int)), this, SLOT(setUpdatesEnabled(int)));
	connect(ui->updateButton, SIGNAL(clicked()), this, SLOT(updateJSON()));
	connect(qsr, SIGNAL(updateStateChanged(Quasars::UpdateState)), this, SLOT(updateStateReceiver(Quasars::UpdateState)));
	connect(qsr, SIGNAL(jsonUpdateComplete(void)), this, SLOT(updateCompleteReceiver(void)));
	connect(ui->updateFrequencySpinBox, SIGNAL(valueChanged(int)), this, SLOT(setUpdateValues(int)));
	refreshUpdateValues(); // fetch values for last updated and so on
	// if the state didn't change, setUpdatesEnabled will not be called, so we force it
	setUpdatesEnabled(ui->internetUpdatesCheckbox->checkState());

	updateTimer = new QTimer(this);
	connect(updateTimer, SIGNAL(timeout()), this, SLOT(refreshUpdateValues()));
	updateTimer->start(7000);

	connect(ui->closeStelWindow, SIGNAL(clicked()), this, SLOT(close()));
	connect(ui->TitleBar, SIGNAL(movedTo(QPoint)), this, SLOT(handleMovedTo(QPoint)));

	connect(ui->restoreDefaultsButton, SIGNAL(clicked()), this, SLOT(restoreDefaults()));
	connect(ui->saveSettingsButton, SIGNAL(clicked()), this, SLOT(saveSettings()));

	// About tab
	setAboutHtml();
	StelGui* gui = dynamic_cast<StelGui*>(StelApp::getInstance().getGui());
	if(gui!=NULL)
		ui->aboutTextBrowser->document()->setDefaultStyleSheet(QString(gui->getStelStyle().htmlStyleSheet));

	updateGuiFromSettings();

}

void QuasarsDialog::setAboutHtml(void)
{
	QString html = "<html><head></head><body>";
	html += "<h2>" + q_("Quasars Plug-in") + "</h2><table width=\"90%\">";
	html += "<tr width=\"30%\"><td><strong>" + q_("Version") + ":</strong></td><td>" + QUASARS_PLUGIN_VERSION + "</td></tr>";
	html += "<tr><td><strong>" + q_("Author") + ":</strong></td><td>Alexander Wolf &lt;alex.v.wolf@gmail.com&gt;</td></tr>";
	html += "</table>";

	html += QString("<p>%1 (<a href=\"%2\">%3</a>)</p>")
			.arg(q_("The Quasars plugin provides visualization of some quasars brighter than visual magnitude 16. The catalogue of quasars was compiled from \"Quasars and Active Galactic Nuclei\" (13th Ed.)"))
			.arg("http://adsabs.harvard.edu/abs/2010A%26A...518A..10V")
			.arg(q_("Veron+ 2010"));

	html += "</ul><p>" + q_("The current catalog contains info about %1 quasars.").arg(qsr->getCountQuasars()) + "</p>";
	html += "<h3>" + q_("Links") + "</h3>";
	html += "<p>" + QString(q_("Support is provided via the Launchpad website.  Be sure to put \"%1\" in the subject when posting.")).arg("Quasars plugin") + "</p>";
	html += "<p><ul>";
	// TRANSLATORS: The numbers contain the opening and closing tag of an HTML link
	html += "<li>" + QString(q_("If you have a question, you can %1get an answer here%2").arg("<a href=\"https://answers.launchpad.net/stellarium\">")).arg("</a>") + "</li>";
	// TRANSLATORS: The numbers contain the opening and closing tag of an HTML link
	html += "<li>" + QString(q_("Bug reports can be made %1here%2.")).arg("<a href=\"https://bugs.launchpad.net/stellarium\">").arg("</a>") + "</li>";
	// TRANSLATORS: The numbers contain the opening and closing tag of an HTML link
	html += "<li>" + q_("If you would like to make a feature request, you can create a bug report, and set the severity to \"wishlist\".") + "</li>";
	// TRANSLATORS: The numbers contain the opening and closing tag of an HTML link
	html += "<li>" + q_("If you want to read full information about this plugin, its history and format of the catalog, you can %1get info here%2.").arg("<a href=\"http://stellarium.org/wiki/index.php/Quasars_plugin\">").arg("</a>") + "</li>";
	html += "</ul></p></body></html>";

	StelGui* gui = dynamic_cast<StelGui*>(StelApp::getInstance().getGui());
	if(gui!=NULL)
	{
		QString htmlStyleSheet(gui->getStelStyle().htmlStyleSheet);
		ui->aboutTextBrowser->document()->setDefaultStyleSheet(htmlStyleSheet);
	}

	ui->aboutTextBrowser->setHtml(html);
}

void QuasarsDialog::refreshUpdateValues(void)
{
	ui->lastUpdateDateTimeEdit->setDateTime(qsr->getLastUpdate());
	ui->updateFrequencySpinBox->setValue(qsr->getUpdateFrequencyDays());
	int secondsToUpdate = qsr->getSecondsToUpdate();
	ui->internetUpdatesCheckbox->setChecked(qsr->getUpdatesEnabled());
	if (!qsr->getUpdatesEnabled())
		ui->nextUpdateLabel->setText(q_("Internet updates disabled"));
	else if (qsr->getUpdateState() == Quasars::Updating)
		ui->nextUpdateLabel->setText(q_("Updating now..."));
	else if (secondsToUpdate <= 60)
		ui->nextUpdateLabel->setText(q_("Next update: < 1 minute"));
	else if (secondsToUpdate < 3600)
		ui->nextUpdateLabel->setText(QString(q_("Next update: %1 minutes")).arg((secondsToUpdate/60)+1));
	else if (secondsToUpdate < 86400)
		ui->nextUpdateLabel->setText(QString(q_("Next update: %1 hours")).arg((secondsToUpdate/3600)+1));
	else
		ui->nextUpdateLabel->setText(QString(q_("Next update: %1 days")).arg((secondsToUpdate/86400)+1));
}

void QuasarsDialog::setUpdateValues(int days)
{
	qsr->setUpdateFrequencyDays(days);
	refreshUpdateValues();
}

void QuasarsDialog::setUpdatesEnabled(int checkState)
{
	bool b = checkState != Qt::Unchecked;
	qsr->setUpdatesEnabled(b);
	ui->updateFrequencySpinBox->setEnabled(b);
	if(b)
		ui->updateButton->setText(q_("Update now"));
	else
		ui->updateButton->setText(q_("Update from files"));

	refreshUpdateValues();
}

void QuasarsDialog::setDistributionEnabled(int checkState)
{
	bool b = checkState != Qt::Unchecked;
	qsr->setDisplayMode(b);
}

void QuasarsDialog::setDisplayAtStartupEnabled(int checkState)
{
	bool b = checkState != Qt::Unchecked;
	qsr->setEnableAtStartup(b);
}

void QuasarsDialog::setDisplayShowQuasarsButton(int checkState)
{
	bool b = checkState != Qt::Unchecked;
	qsr->setFlagShowQuasarsButton(b);
}

void QuasarsDialog::updateStateReceiver(Quasars::UpdateState state)
{
	//qDebug() << "QuasarsDialog::updateStateReceiver got a signal";
	if (state==Quasars::Updating)
		ui->nextUpdateLabel->setText(q_("Updating now..."));
	else if (state==Quasars::DownloadError || state==Quasars::OtherError)
	{
		ui->nextUpdateLabel->setText(q_("Update error"));
		updateTimer->start();  // make sure message is displayed for a while...
	}
}

void QuasarsDialog::updateCompleteReceiver(void)
{
	ui->nextUpdateLabel->setText(QString(q_("Quasars is updated")));
	// display the status for another full interval before refreshing status
	updateTimer->start();
	ui->lastUpdateDateTimeEdit->setDateTime(qsr->getLastUpdate());
	QTimer *timer = new QTimer(this);
	connect(timer, SIGNAL(timeout()), this, SLOT(refreshUpdateValues()));
}

void QuasarsDialog::restoreDefaults(void)
{
	qDebug() << "Quasars::restoreDefaults";
	qsr->restoreDefaults();
	qsr->readSettingsFromConfig();
	updateGuiFromSettings();
}

void QuasarsDialog::updateGuiFromSettings(void)
{
	ui->internetUpdatesCheckbox->setChecked(qsr->getUpdatesEnabled());
	refreshUpdateValues();
}

void QuasarsDialog::saveSettings(void)
{
	qsr->saveSettingsToConfig();
}

void QuasarsDialog::updateJSON(void)
{
	if(qsr->getUpdatesEnabled())
	{
		qsr->updateJSON();
	}
}
