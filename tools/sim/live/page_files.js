'use strict';
/**
 * The files of the live view's page, as the server sends them: page.html,
 * with each section kept in page/ put in where its `part:` comment stands,
 * and the style and scripts in page/. All are read on every request, so an
 * edit shows at the next reload. Nothing else on disk is served.
 */
const fs = require('node:fs');
const path = require('node:path');

const PAGE_PATH = path.join(__dirname, 'page.html');
const PARTS_DIR = path.join(__dirname, 'page');
// A line of page.html that stands for a file of PARTS_DIR
const PART_MARK = /^[ \t]*<!-- part: ([\w-]+\.html) -->$/gm;
// The only other files served: a name with no folder in it, from PARTS_DIR
const ASSET_URL = /^\/([\w-]+\.(css|js))$/;
const ASSET_TYPES = { css: 'text/css', js: 'text/javascript' };

function reply(res, status, text) {
    res.writeHead(status, { 'Content-Type': 'text/plain; charset=utf-8' });
    res.end(text);
}

function servePage(res) {
    res.writeHead(200, {
        'Content-Type': 'text/html; charset=utf-8',
        'Cache-Control': 'no-store',
    });
    res.end(fs.readFileSync(PAGE_PATH, 'utf8').replace(PART_MARK,
        (mark, name) => fs.readFileSync(path.join(PARTS_DIR, name), 'utf8').trimEnd()));
}

// Read on every request, as the page is, so an edit shows at the next reload
function serveAsset(res, name, extension) {
    let text;

    try {
        text = fs.readFileSync(path.join(PARTS_DIR, name));
    } catch (error) {
        reply(res, 404, 'Not found');
        return;
    }
    res.writeHead(200, {
        'Content-Type': `${ASSET_TYPES[extension]}; charset=utf-8`,
        'Cache-Control': 'no-store',
    });
    res.end(text);
}

module.exports = { reply, servePage, serveAsset, ASSET_URL };
