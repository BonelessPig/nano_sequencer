// A reporter for `node --test` that prints one line of JSON: how many
// scenarios each file ran, and how many of them failed. tools/figures/figures.py
// runs the emulated board's scenarios with it to count them.
import path from 'node:path';

export default async function* scenarioCounts(source) {
  const counts = {};
  let failed = 0;

  for await (const event of source) {
    const isResult = event.type === 'test:pass' || event.type === 'test:fail';
    // A suite only groups scenarios; it is not one itself
    if (!isResult || event.data.details?.type === 'suite') {
      continue;
    }
    const file = path.basename(event.data.file ?? 'unknown');
    counts[file] = (counts[file] ?? 0) + 1;
    if (event.type === 'test:fail') {
      failed += 1;
    }
  }

  yield `${JSON.stringify({ counts, failed })}\n`;
}
