/*
 * Copyright (c) 2026, NinjaLibrary
 * All rights reserved.
 */

import QtQuick
import QtTest
import NinjaLibrary
import Kaakao 1.0

TestCase {
    name: "StarRatingRowTests"
    width: 640
    height: 480
    visible: true

    Component {
        id: starRatingRowComponent
        StarRatingRow {}
    }

    function test_star_rating_row_defaults() {
        let row = starRatingRowComponent.createObject(this);
        verify(row !== null, "StarRatingRow should be created");

        compare(row.rating, 0, "Default rating should be 0");
        compare(row.hoveredIndex, -1, "Default hoveredIndex should be -1");

        row.destroy();
    }

    function test_star_rating_row_interaction() {
        let row = starRatingRowComponent.createObject(this);
        verify(row !== null, "StarRatingRow should be created");

        let spy = createTemporaryQmlObject("import QtTest; SignalSpy {}", this);
        spy.target = row;
        spy.signalName = "ratingSelected";

        // Find the MouseAreas inside the Repeater
        function findMouseAreas(parent, list) {
            if (!parent) return;
            if (parent.toString().indexOf("QQuickMouseArea") >= 0) {
                list.push(parent);
            }
            if (parent.children) {
                for (let i = 0; i < parent.children.length; ++i) {
                    findMouseAreas(parent.children[i], list);
                }
            }
        }
        let mouseAreas = [];
        findMouseAreas(row, mouseAreas);
        compare(mouseAreas.length, 5, "Should have 5 star mouse areas");

        // Hover over the third star (index 2)
        mouseAreas[2].entered();
        compare(row.hoveredIndex, 2, "hoveredIndex should update on enter");

        mouseAreas[2].exited();
        compare(row.hoveredIndex, -1, "hoveredIndex should reset on exit");

        // Click the third star
        mouseClick(mouseAreas[2]);
        compare(spy.count, 1, "Should emit ratingSelected");
        compare(spy.signalArguments[0][0], 3, "New rating should be 3");
        compare(row.rating, 3, "Rating property should be 3");

        // Click the third star again to clear (toggle rating)
        mouseClick(mouseAreas[2]);
        compare(spy.count, 2, "Should emit ratingSelected again");
        compare(spy.signalArguments[1][0], 0, "Rating should toggle to 0");
        compare(row.rating, 0, "Rating property should toggle to 0");

        row.destroy();
        spy.destroy();
    }
}
