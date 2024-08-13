# -*- coding: utf-8 -*-

################################################################################
## Form generated from reading UI file 'MainWindow.ui'
##
## Created by: Qt User Interface Compiler version 6.7.2
##
## WARNING! All changes made in this file will be lost when recompiling UI file!
################################################################################

from PySide6.QtCore import (QCoreApplication, QDate, QDateTime, QLocale,
    QMetaObject, QObject, QPoint, QRect,
    QSize, QTime, QUrl, Qt)
from PySide6.QtGui import (QBrush, QColor, QConicalGradient, QCursor,
    QFont, QFontDatabase, QGradient, QIcon,
    QImage, QKeySequence, QLinearGradient, QPainter,
    QPalette, QPixmap, QRadialGradient, QTransform)
from PySide6.QtWidgets import (QApplication, QCheckBox, QFrame, QGridLayout,
    QLabel, QPlainTextEdit, QPushButton, QSizePolicy,
    QSpacerItem, QSpinBox, QStackedWidget, QWidget)

class Ui_Form(object):
    def setupUi(self, Form):
        if not Form.objectName():
            Form.setObjectName(u"Form")
        Form.setWindowModality(Qt.WindowModality.NonModal)
        Form.resize(1368, 389)
        Form.setMinimumSize(QSize(1368, 389))
        Form.setMaximumSize(QSize(1368, 389))
        palette = QPalette()
        brush = QBrush(QColor(85, 85, 255, 255))
        brush.setStyle(Qt.SolidPattern)
        palette.setBrush(QPalette.Active, QPalette.Highlight, brush)
        palette.setBrush(QPalette.Active, QPalette.Link, brush)
        palette.setBrush(QPalette.Active, QPalette.Accent, brush)
        palette.setBrush(QPalette.Inactive, QPalette.Highlight, brush)
        palette.setBrush(QPalette.Inactive, QPalette.Link, brush)
        palette.setBrush(QPalette.Inactive, QPalette.Accent, brush)
        palette.setBrush(QPalette.Disabled, QPalette.Link, brush)
        palette.setBrush(QPalette.Disabled, QPalette.Accent, brush)
        Form.setPalette(palette)
        font = QFont()
        font.setFamilies([u"RobotoMono Nerd Font"])
        font.setPointSize(11)
        Form.setFont(font)
        Form.setCursor(QCursor(Qt.CursorShape.ArrowCursor))
        Form.setAcceptDrops(False)
        icon = QIcon()
        icon.addFile(u"pallet.ico", QSize(), QIcon.Mode.Normal, QIcon.State.On)
        Form.setWindowIcon(icon)
        Form.setAutoFillBackground(True)
        Form.setInputMethodHints(Qt.InputMethodHint.ImhNone)
        self.stackedWidget = QStackedWidget(Form)
        self.stackedWidget.setObjectName(u"stackedWidget")
        self.stackedWidget.setEnabled(True)
        self.stackedWidget.setGeometry(QRect(0, 0, 1366, 389))
        sizePolicy = QSizePolicy(QSizePolicy.Policy.Fixed, QSizePolicy.Policy.Fixed)
        sizePolicy.setHorizontalStretch(0)
        sizePolicy.setVerticalStretch(0)
        sizePolicy.setHeightForWidth(self.stackedWidget.sizePolicy().hasHeightForWidth())
        self.stackedWidget.setSizePolicy(sizePolicy)
        self.stackedWidget.setLayoutDirection(Qt.LayoutDirection.LeftToRight)
        self.stackedWidget.setFrameShape(QFrame.Shape.NoFrame)
        self.MainMenu = QWidget()
        self.MainMenu.setObjectName(u"MainMenu")
        self.settings = QPushButton(self.MainMenu)
        self.settings.setObjectName(u"settings")
        self.settings.setGeometry(QRect(10, 10, 61, 61))
        sizePolicy.setHeightForWidth(self.settings.sizePolicy().hasHeightForWidth())
        self.settings.setSizePolicy(sizePolicy)
        self.settings.setAutoFillBackground(False)
        self.settings.setFlat(True)
        self.checkBox = QCheckBox(self.MainMenu)
        self.checkBox.setObjectName(u"checkBox")
        self.checkBox.setEnabled(False)
        self.checkBox.setGeometry(QRect(270, 250, 131, 20))
        self.pushButton_3 = QPushButton(self.MainMenu)
        self.pushButton_3.setObjectName(u"pushButton_3")
        self.pushButton_3.setEnabled(False)
        self.pushButton_3.setGeometry(QRect(620, 250, 121, 24))
        self.gridLayoutWidget = QWidget(self.MainMenu)
        self.gridLayoutWidget.setObjectName(u"gridLayoutWidget")
        self.gridLayoutWidget.setGeometry(QRect(270, 20, 691, 176))
        self.gridLayout = QGridLayout(self.gridLayoutWidget)
        self.gridLayout.setObjectName(u"gridLayout")
        self.gridLayout.setContentsMargins(0, 0, 0, 0)
        self.spinBox = QSpinBox(self.gridLayoutWidget)
        self.spinBox.setObjectName(u"spinBox")
        self.spinBox.setEnabled(False)
        sizePolicy1 = QSizePolicy(QSizePolicy.Policy.Minimum, QSizePolicy.Policy.Minimum)
        sizePolicy1.setHorizontalStretch(0)
        sizePolicy1.setVerticalStretch(0)
        sizePolicy1.setHeightForWidth(self.spinBox.sizePolicy().hasHeightForWidth())
        self.spinBox.setSizePolicy(sizePolicy1)
        self.spinBox.setMinimumSize(QSize(0, 39))

        self.gridLayout.addWidget(self.spinBox, 1, 1, 1, 1)

        self.label_3 = QLabel(self.gridLayoutWidget)
        self.label_3.setObjectName(u"label_3")
        self.label_3.setLayoutDirection(Qt.LayoutDirection.LeftToRight)

        self.gridLayout.addWidget(self.label_3, 2, 0, 1, 1)

        self.EingabeStartlage_2 = QPlainTextEdit(self.gridLayoutWidget)
        self.EingabeStartlage_2.setObjectName(u"EingabeStartlage_2")
        self.EingabeStartlage_2.setEnabled(False)
        self.EingabeStartlage_2.setMinimumSize(QSize(0, 39))

        self.gridLayout.addWidget(self.EingabeStartlage_2, 2, 1, 1, 1)

        self.label_4 = QLabel(self.gridLayoutWidget)
        self.label_4.setObjectName(u"label_4")
        self.label_4.setLayoutDirection(Qt.LayoutDirection.LeftToRight)

        self.gridLayout.addWidget(self.label_4, 3, 0, 1, 1)

        self.EingabeStartlage_3 = QPlainTextEdit(self.gridLayoutWidget)
        self.EingabeStartlage_3.setObjectName(u"EingabeStartlage_3")
        self.EingabeStartlage_3.setEnabled(False)
        self.EingabeStartlage_3.setMinimumSize(QSize(0, 39))

        self.gridLayout.addWidget(self.EingabeStartlage_3, 3, 1, 1, 1)

        self.label = QLabel(self.gridLayoutWidget)
        self.label.setObjectName(u"label")
        self.label.setLayoutDirection(Qt.LayoutDirection.LeftToRight)

        self.gridLayout.addWidget(self.label, 0, 0, 1, 1)

        self.EingabePallettenplan = QPlainTextEdit(self.gridLayoutWidget)
        self.EingabePallettenplan.setObjectName(u"EingabePallettenplan")
        self.EingabePallettenplan.setMinimumSize(QSize(0, 39))
        self.EingabePallettenplan.viewport().setProperty("cursor", QCursor(Qt.CursorShape.IBeamCursor))
        self.EingabePallettenplan.setInputMethodHints(Qt.InputMethodHint.ImhNone)
        self.EingabePallettenplan.setOverwriteMode(False)

        self.gridLayout.addWidget(self.EingabePallettenplan, 0, 1, 1, 1)

        self.label_2 = QLabel(self.gridLayoutWidget)
        self.label_2.setObjectName(u"label_2")
        self.label_2.setLayoutDirection(Qt.LayoutDirection.LeftToRight)

        self.gridLayout.addWidget(self.label_2, 1, 0, 1, 1)

        self.LadePallettenplan = QPushButton(self.gridLayoutWidget)
        self.LadePallettenplan.setObjectName(u"LadePallettenplan")
        sizePolicy1.setHeightForWidth(self.LadePallettenplan.sizePolicy().hasHeightForWidth())
        self.LadePallettenplan.setSizePolicy(sizePolicy1)
        self.LadePallettenplan.setCursor(QCursor(Qt.CursorShape.PointingHandCursor))

        self.gridLayout.addWidget(self.LadePallettenplan, 0, 2, 1, 1)

        self.pushButton_2 = QPushButton(self.MainMenu)
        self.pushButton_2.setObjectName(u"pushButton_2")
        self.pushButton_2.setEnabled(False)
        self.pushButton_2.setGeometry(QRect(420, 250, 171, 24))
        self.stackedWidget.addWidget(self.MainMenu)
        self.RoboParameter = QWidget()
        self.RoboParameter.setObjectName(u"RoboParameter")
        self.stackedWidget.addWidget(self.RoboParameter)
        self.Settings = QWidget()
        self.Settings.setObjectName(u"Settings")
        self.gridLayoutWidget_2 = QWidget(self.Settings)
        self.gridLayoutWidget_2.setObjectName(u"gridLayoutWidget_2")
        self.gridLayoutWidget_2.setGeometry(QRect(20, 20, 1311, 341))
        self.gridLayout_2 = QGridLayout(self.gridLayoutWidget_2)
        self.gridLayout_2.setObjectName(u"gridLayout_2")
        self.gridLayout_2.setContentsMargins(0, 0, 0, 0)
        self.Button_OpenTerminal = QPushButton(self.gridLayoutWidget_2)
        self.Button_OpenTerminal.setObjectName(u"Button_OpenTerminal")

        self.gridLayout_2.addWidget(self.Button_OpenTerminal, 1, 0, 1, 1)

        self.Button_OpenExplorer = QPushButton(self.gridLayoutWidget_2)
        self.Button_OpenExplorer.setObjectName(u"Button_OpenExplorer")

        self.gridLayout_2.addWidget(self.Button_OpenExplorer, 0, 0, 1, 1)

        self.horizontalSpacer = QSpacerItem(40, 20, QSizePolicy.Policy.Expanding, QSizePolicy.Policy.Minimum)

        self.gridLayout_2.addItem(self.horizontalSpacer, 0, 1, 1, 1)

        self.stackedWidget.addWidget(self.Settings)

        self.retranslateUi(Form)

        self.stackedWidget.setCurrentIndex(0)
        self.settings.setDefault(False)


        QMetaObject.connectSlotsByName(Form)
    # setupUi

    def retranslateUi(self, Form):
        Form.setWindowTitle(QCoreApplication.translate("Form", u"Palletierer", None))
        self.settings.setText("")
        self.checkBox.setText(QCoreApplication.translate("Form", u"Einzelpaket", None))
        self.pushButton_3.setText(QCoreApplication.translate("Form", u"Daten Senden", None))
        self.label_3.setText(QCoreApplication.translate("Form", u"<html><head/><body><p align=\"right\">Kartonh\u00f6he: </p></body></html>", None))
        self.EingabeStartlage_2.setPlainText("")
        self.label_4.setText(QCoreApplication.translate("Form", u"<html><head/><body><p align=\"right\">Gewicht: </p></body></html>", None))
        self.EingabeStartlage_3.setPlainText("")
        self.label.setText(QCoreApplication.translate("Form", u"<html><head/><body><p align=\"right\">Palletierplan: </p></body></html>", None))
        self.EingabePallettenplan.setPlainText("")
        self.EingabePallettenplan.setPlaceholderText(QCoreApplication.translate("Form", u"e.g. 699-00280", None))
        self.label_2.setText(QCoreApplication.translate("Form", u"<html><head/><body><p align=\"right\">Startlage: </p></body></html>", None))
        self.LadePallettenplan.setText(QCoreApplication.translate("Form", u"Laden", None))
        self.pushButton_2.setText(QCoreApplication.translate("Form", u"Parameter Roboter", None))
        self.Button_OpenTerminal.setText(QCoreApplication.translate("Form", u"Open Terminal", None))
        self.Button_OpenExplorer.setText(QCoreApplication.translate("Form", u"Open Explorer", None))
    # retranslateUi

