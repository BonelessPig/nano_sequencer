'use strict';
// The trace of the outputs and inputs, and what readies a canvas
// ---- Trace ----

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

function drawTrace() {
    const { context, width, height, colour } = openCanvas(el('trace'));
    const cvHeight = height * 0.42;
    const pinHeight = height * 0.08;
    // The four rows under the CV: where each starts, as a part of the height
    const rows = { gate: 0.48, clock: 0.61, clockIn: 0.74, resetIn: 0.87 };
    const labelDrop = height * 0.06;
    const cvMax = Math.max(CV_SCALE_MIN_MV, ...trace.cv.map((point) => point[1]));
    const cvFullScale = (cvMax <= CV_SCALE_MIN_MV)
        ? CV_SCALE_MIN_MV : Math.ceil(cvMax / CV_SCALE_STEP_MV) * CV_SCALE_STEP_MV;

    context.fillStyle = colour('--dim');
    context.fillText(`CV, 0 to ${(cvFullScale / 1000).toFixed(2)} V`, 4, 12);
    context.fillText('Gate', 4, (height * rows.gate) + labelDrop);
    context.fillText('Clock out', 4, (height * rows.clock) + labelDrop);
    context.fillText('Clock in', 4, (height * rows.clockIn) + labelDrop);
    context.fillText('Reset in', 4, (height * rows.resetIn) + labelDrop);
    if (el('trace-axes').checked) {
        const volts = [];

        for (let mv = 0; mv <= cvFullScale; mv += CV_SCALE_STEP_MV) {
            volts.push([mv / cvFullScale, `${mv / 1000} V`]);
        }
        drawAxes(context, colour, 16, cvHeight - 16, width, volts, []);
        // The seconds go down through every row, named under the last
        drawAxes(context, colour, 16, height - 16, width, [], secondsBack());
    }
    drawSignal(context, trace.cv, colour('--cv'), 16, cvHeight - 16, cvFullScale, width);
    drawSignal(context, trace.gate, colour('--gate'),
        height * rows.gate, pinHeight, 1, width);
    drawSignal(context, trace.clock, colour('--clock'),
        height * rows.clock, pinHeight, 1, width);
    drawEdges(context, trace.clockIn, colour('--in'),
        height * rows.clockIn, pinHeight, width);
    drawEdges(context, trace.resetIn, colour('--in'),
        height * rows.resetIn, pinHeight, width);
    requestAnimationFrame(drawTrace);
}
