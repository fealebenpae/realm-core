const core = require('@actions/core');
const { issueCommand } = require('@actions/core/lib/command');
const fs = require('fs');
const readline = require('readline');

const logFilePath = core.getInput('log-file-path', { required: true });

let format;
switch (core.getInput('tool', { required: true })) {
    case 'msvc':
        format = {
            regexp: /^([^\s].*)\((\d+|\d+,\d+|\d+,\d+,\d+,\d+)\):\s+(error|warning)\s+(C\d+\s*:\s*.*)$/,
            file: 1,
            location: 2,
            severity: 3,
            message: 4
        };
        break;
    case 'gcc':
    case 'clang':
        format = {
            regexp: /^(.*):(\d+):(\d+):\s+(?:fatal\s+)?(warning|error):\s+(.*)$/,
            file: 1,
            line: 2,
            column: 3,
            severity: 4,
            message: 5
        };
        break;
    default:
        throw new Error('Unsupported tool');
}

const reader = readline.createInterface(fs.createReadStream(logFilePath));
reader.on('line', line => {
    const match = format.regexp.exec(line);
    if (match != null) {
        const file = match[format.file];
        let line, col = 0;
        if ('location' in format) {
            [line, col] = match[format.location].split(',');
        } else {
            line = match[format.line];
            col = match[format.column];
        }
        issueCommand(match[format.severity], { file, line, col }, match[format.message]);
    }
});
