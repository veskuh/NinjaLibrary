/*
 * Copyright (c) 2026, NinjaLibrary
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

import QtQuick
import QtQuick.Controls

Item {
    id: pane

    property bool collapsed: false
    property real minWidth: 150
    property real preferredWidth: 200
    property real maxWidth: 300
    property bool isAnimating: false

    states: [
        State {
            name: "collapsed"
            when: pane.collapsed
            PropertyChanges {
                target: pane
                SplitView.minimumWidth: 0
                SplitView.preferredWidth: 0
                SplitView.maximumWidth: 0
            }
        },
        State {
            name: "expanded"
            when: !pane.collapsed
            PropertyChanges {
                target: pane
                SplitView.minimumWidth: pane.minWidth
                SplitView.preferredWidth: pane.preferredWidth
                SplitView.maximumWidth: pane.maxWidth
            }
        }
    ]

    transitions: [
        Transition {
            from: "expanded"
            to: "collapsed"
            SequentialAnimation {
                ScriptAction { script: pane.isAnimating = true }
                NumberAnimation {
                    properties: "SplitView.preferredWidth,SplitView.minimumWidth,SplitView.maximumWidth"
                    duration: 200
                    easing.type: Easing.InOutQuad
                }
                ScriptAction { script: pane.isAnimating = false }
            }
        },
        Transition {
            from: "collapsed"
            to: "expanded"
            SequentialAnimation {
                ScriptAction { script: pane.isAnimating = true }
                NumberAnimation {
                    properties: "SplitView.preferredWidth,SplitView.minimumWidth,SplitView.maximumWidth"
                    duration: 200
                    easing.type: Easing.InOutQuad
                }
                ScriptAction { script: pane.isAnimating = false }
            }
        }
    ]
}
