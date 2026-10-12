'use strict';
// The chart of every signal, the board's own bands of it, and what readies
// a canvas. The gear's bands are in gear.js
// ---- Chart ----

// Drops what has scrolled off, keeping the one point that says what the
// level was at the left edge
function trim(points) {
    while ((points.length > 1) && (points[1][0] < (nowMs - TRACE_WINDOW_MS))) {
        points.shift();
    }
}

// Where a chip time is across a trace this wide, the present at its right
const traceX = (ms, width) => width * (1 - ((nowMs - ms) / TRACE_WINDOW_MS));

// Draws one signal as steps, in a band of the canvas from top to top + height
function drawSignal(context, points, colour, top, height, fullScale, width) {
    const x = (ms) => traceX(ms, width);
    const y = (value) => top + (height * (1 - (value / fullScale)));

    trim(points);
    if (points.length === 0) {
        return;
    }
    context.strokeStyle = colour;
    context.beginPath();
    context.moveTo(Math.max(0, x(points[0][0])), y(points[0][1]));
    for (let i = 1; i < points.length; i++) {
        context.lineTo(x(points[i][0]), y(points[i - 1][1]));
        context.lineTo(x(points[i][0]), y(points[i][1]));
    }
    context.lineTo(width, y(points[points.length - 1][1]));
    context.stroke();
}

// Draws one mark for each edge that arrived at an input, in the same kind
// of band. Only the instant of the edge is known, not how long it was high
function drawEdges(context, times, colour, top, height, width) {
    while ((times.length > 0) && (times[0] < (nowMs - TRACE_WINDOW_MS))) {
        times.shift();
    }
    context.strokeStyle = colour;
    context.beginPath();
    for (const ms of times) {
        const x = traceX(ms, width);

        context.moveTo(x, top);
        context.lineTo(x, top + height);
    }
    context.stroke();
}

// The lines down a band that is the trace's window wide: one at each tick
// of time back from the present, each as [part of the way across, label]
function secondsBack() {
    const times = [];

    for (let ms = AXIS_TICK_MS; ms < TRACE_WINDOW_MS; ms += AXIS_TICK_MS) {
        times.push([1 - (ms / TRACE_WINDOW_MS), `-${ms / MS_PER_S} s`]);
    }
    return times;
}

// A band's axes, drawn faint and dashed before its signal: a line across
// at each of `levels`, named at the right, and a line down at each of
// `times`, named at the band's foot. Each is [part, label], the part 0 to
// 1 up the band or across it
function drawAxes(context, colour, top, height, width, levels, times) {
    const y = (part) => top + (height * (1 - part));

    context.save();
    context.strokeStyle = colour('--dim');
    context.fillStyle = colour('--dim');
    context.globalAlpha = AXIS_ALPHA;
    context.lineWidth = 1;
    context.setLineDash(AXIS_DASH);
    context.beginPath();
    for (const [part] of levels) {
        context.moveTo(0, y(part));
        context.lineTo(width, y(part));
    }
    for (const [part] of times) {
        context.moveTo(width * part, top);
        context.lineTo(width * part, top + height);
    }
    context.stroke();
    context.globalAlpha = 1;
    for (const [part, label] of times) {
        context.fillText(label, (width * part) + 3, top + height - 2);
    }
    context.textAlign = 'right';
    for (const [part, label] of levels) {
        // Over its line, but under it at the top of the band
        context.fillText(label, width - 4,
            y(part) + ((part > AXIS_LABEL_UNDER) ? AXIS_LABEL_DROP_PX : -3));
    }
    context.restore();
}

// Makes a canvas ready for a frame: sized to its box at the screen's own
// pixels, cleared, with the line and the lettering every drawing here uses.
// `colour` gives one of the style's colours by name, read from the style
// once a frame however often it is asked for
function openCanvas(canvas) {
    const context = canvas.getContext('2d');
    const scale = window.devicePixelRatio || 1;
    const width = canvas.clientWidth;
    const height = canvas.clientHeight;
    const style = getComputedStyle(document.documentElement);
    const read = {};

    if ((canvas.width !== Math.round(width * scale))
        || (canvas.height !== Math.round(height * scale))) {
        canvas.width = Math.round(width * scale);
        canvas.height = Math.round(height * scale);
    }
    context.setTransform(scale, 0, 0, scale, 0, 0);
    context.clearRect(0, 0, width, height);
    context.lineWidth = 1.5;
    context.font = '11px system-ui, sans-serif';
    return { context, width, height,
        colour: (name) => {
            read[name] ??= style.getPropertyValue(name).trim();
            return read[name];
        } };
}

// Where everything is on a chart this high. `bands` has each band by name
// as { label, top, height }: where its name is written, over it, and its
// own room. `headings` has each section's as { text, y }
function chartLayout(height) {
    const all = CHART_SECTIONS.flatMap((section) => section[1]);
    const shares = all.reduce((sum, band) => sum + band[1], 0);
    const unit = (height - (CHART_SECTIONS.length * CHART_HEADING_PX)
        - (all.length * (CHART_LABEL_PX + CHART_GAP_PX))) / shares;
    const bands = {};
    const headings = [];
    let y = 0;

    for (const [text, members] of CHART_SECTIONS) {
        headings.push({ text, y: y + CHART_HEADING_PX - 6 });
        y += CHART_HEADING_PX;
        for (const [name, share] of members) {
            bands[name] = { label: y + CHART_LABEL_PX - 4, top: y + CHART_LABEL_PX,
                height: unit * share };
            y += CHART_LABEL_PX + (unit * share) + CHART_GAP_PX;
        }
    }
    return { bands, headings };
}

// The chart's section headings, each with a rule over it but the first
function drawHeadings({ context, width, colour }, headings) {
    context.save();
    context.font = '600 12px system-ui, sans-serif';
    context.fillStyle = colour('--ink');
    context.strokeStyle = colour('--line');
    context.lineWidth = 1;
    context.beginPath();
    for (const { text, y } of headings) {
        context.fillText(text, 4, y);
        if (y > CHART_HEADING_PX) {
            context.moveTo(0, y - CHART_HEADING_PX + 8);
            context.lineTo(width, y - CHART_HEADING_PX + 8);
        }
    }
    context.stroke();
    context.restore();
}

// The chart's key, at the right of its first heading: each kind of signal
// named in its colour, the last of them against the right edge
function drawKey({ context, width, colour }, y) {
    let x = width - 4;

    context.save();
    context.textAlign = 'right';
    for (const [name, text] of [...CHART_KINDS].reverse()) {
        context.fillStyle = colour(name);
        context.fillText(`■ ${text}`, x, y);
        x -= context.measureText(`■ ${text}`).width + CHART_KEY_GAP_PX;
    }
    context.restore();
}

// The seconds back from the present, as lines down a section's bands that
// are the window wide, from the first to the last, named under the last
function drawSeconds({ context, width, colour }, first, last) {
    drawAxes(context, colour, first.top, last.top + last.height - first.top,
        width, [], secondsBack());
}

// The one chart, in two sections: the board's signals over the window,
// the clocks first, and the gear's bands, which gear.js draws. `time` is
// the frame's, in ms
function drawTrace(time) {
    const chart = openCanvas(el('trace'));
    const { context, width, colour } = chart;
    const { bands, headings } = chartLayout(chart.height);
    const hasAxes = el('trace-axes').checked;
    const cvMax = Math.max(CV_SCALE_MIN_MV, ...trace.cv.map((point) => point[1]));
    const cvFullScale = (cvMax <= CV_SCALE_MIN_MV)
        ? CV_SCALE_MIN_MV : Math.ceil(cvMax / CV_SCALE_STEP_MV) * CV_SCALE_STEP_MV;

    drawHeadings(chart, headings);
    drawKey(chart, headings[0].y);
    context.fillStyle = colour('--in');
    context.fillText('Clock in', 4, bands.clockIn.label);
    context.fillText('Reset in', 4, bands.resetIn.label);
    context.fillStyle = colour('--out');
    context.fillText('Clock out', 4, bands.clock.label);
    context.fillText('Gate', 4, bands.gate.label);
    context.fillText(`Pitch CV, 0 to ${(cvFullScale / 1000).toFixed(2)} V`,
        4, bands.cv.label);
    if (hasAxes) {
        const volts = [];

        for (let mv = 0; mv <= cvFullScale; mv += CV_SCALE_STEP_MV) {
            volts.push([mv / cvFullScale, `${mv / 1000} V`]);
        }
        drawAxes(context, colour, bands.cv.top, bands.cv.height, width, volts, []);
        drawSeconds(chart, bands.clockIn, bands.cv);
    }
    drawSignal(context, trace.cv, colour('--out'),
        bands.cv.top, bands.cv.height, cvFullScale, width);
    drawSignal(context, trace.gate, colour('--out'),
        bands.gate.top, bands.gate.height, 1, width);
    drawSignal(context, trace.clock, colour('--out'),
        bands.clock.top, bands.clock.height, 1, width);
    drawEdges(context, trace.clockIn, colour('--in'),
        bands.clockIn.top, bands.clockIn.height, width);
    drawEdges(context, trace.resetIn, colour('--in'),
        bands.resetIn.top, bands.resetIn.height, width);
    drawGearBands(chart, bands, time, hasAxes);
    requestAnimationFrame(drawTrace);
}
