#!/usr/bin/env node

const { codeGenWithLocation } = require('shift-codegen');
process.stdin.setEncoding('utf8');

var ast_json = ""

process.stdin.on('readable', () => {
  let chunk;
  while ((chunk = process.stdin.read()) !== null) {
      ast_json += chunk;
  }
});

process.stdin.on('end', () => {
   const { source, locations} = codeGenWithLocation(JSON.parse(ast_json));
   process.stdout.write(JSON.stringify(eval(source)));
});

