/******************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd
** All rights reserved.
** For any questions to The Qt Company, please use contact form at http://qt.io
**
** This file is part of the Qt Virtual Keyboard module.
**
** Licensees holding valid commercial license for Qt may use this file in
** accordance with the Qt License Agreement provided with the Software
** or, alternatively, in accordance with the terms contained in a written
** agreement between you and The Qt Company.
**
** If you have questions regarding the use of this file, please use
** contact form at http://qt.io
**
******************************************************************************/

import QtQuick 2.0

/*!
    \qmltype BackspaceKey
    \inqmlmodule QtQuick.Enterprise.VirtualKeyboard
    \ingroup qtvirtualkeyboard-qml
    \inherits BaseKey

    \brief Backspace key for keyboard layouts.

    Sends a backspace key for input method processing.
    This key is repeatable.
*/

BaseKey {
    key: Qt.Key_Backspace
    repeat: true
    functionKey: true
    keyPanelDelegate: keyboard.style ? keyboard.style.backspaceKeyPanel : undefined
}
