#!/usr/bin/env node

const { parseScriptWithLocation } = require('shift-parser');
process.stdin.setEncoding('utf8');

var js_code;

process.stdin.on('readable', () => {
  let chunk;
  while ((chunk = process.stdin.read()) !== null) {
      js_code += chunk;
  }
});

process.stdin.on('end', () => {
    const ast = parseScriptWithLocation(js_code);
    process.stdout.write(JSON.stringify(ast));
});
