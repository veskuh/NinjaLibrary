/*
 * Copyright (c) 2026, NinjaLibrary
 * All rights reserved.
 */

import QtQuick
import Kaakao 1.0

Row {
    id: ratingRow
    spacing: 4

    property int hoveredIndex: -1
    property int rating: 0

    signal ratingSelected(int newRating)

    Repeater {
        model: 5
        KaakaoLabel {
            text: "★"
            font.pixelSize: 22
            color: {
                var active = false;
                if (ratingRow.hoveredIndex !== -1) {
                    active = index <= ratingRow.hoveredIndex;
                } else {
                    active = index < ratingRow.rating;
                }
                return active ? "#f1c40f" : (Theme.isDarkMode ? "#333" : "#ccc");
            }
            opacity: 1.0
            scale: starMouseArea.containsMouse ? 1.2 : 1.0

            Behavior on scale {
                NumberAnimation {
                    duration: 100
                }
            }
            Behavior on color {
                ColorAnimation {
                    duration: 100
                }
            }

            MouseArea {
                id: starMouseArea
                anchors.fill: parent
                hoverEnabled: true
                onEntered: ratingRow.hoveredIndex = index
                onExited: ratingRow.hoveredIndex = -1
                onClicked: {
                    var newRating = index + 1;
                    if (ratingRow.rating === newRating) {
                        newRating = 0;
                    }
                    ratingRow.rating = newRating;
                    ratingRow.ratingSelected(newRating);
                }
            }
        }
    }
}
