/*
 * Copyright (c) 2026, NinjaLibrary
 * All rights reserved.
 *
 * Standalone JS helper for tag-text parsing so it can be imported and tested
 * independently of the Inspector QML component.
 */

.pragma library

/**
 * Splits a comma-separated tag string, trims whitespace from each part,
 * and deduplicates the result (preserving first-occurrence order).
 *
 * @param {string} rawText  Raw user input, e.g. "  work , School , work "
 * @returns {string[]}      Unique, trimmed, non-empty tag strings
 */
function parseTagsText(rawText) {
    var parts = rawText.split(",");
    var tags = [];
    for (var i = 0; i < parts.length; i++) {
        var part = parts[i].trim();
        if (part !== "" && tags.indexOf(part) === -1) {
            tags.push(part);
        }
    }
    return tags;
}
