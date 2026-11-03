#include "exteraless/plugins/scanner.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QProcess>
#include <QStandardPaths>
#include <QStringList>

namespace Exteraless::Plugins {
namespace {

constexpr auto kStartTimeout = 5000;
constexpr auto kScanTimeout = 10000;
constexpr auto kKillTimeout = 1000;

constexpr auto kScanScript = R"PY(import ast
import json
import os
import re
import stat
import sys

ID_RE = re.compile(r'^[A-Za-z][A-Za-z0-9_-]{1,31}$')
KNOWN = {
    'ui', 'messages.read', 'messages.send', 'network', 'files',
    'intents', 'settings', 'hooks', 'native',
}
MAX_BYTES = 2 * 1024 * 1024
SUPPORTED = ('.plugin', '.py')
UNSUPPORTED = ('.elyx', '.eaf')
STRINGS = {
    '__id__': 'id',
    '__name__': 'name',
    '__description__': 'description',
    '__author__': 'author',
    '__version__': 'version',
}


def empty(name):
    return {
        'file': name,
        'id': '',
        'name': '',
        'description': '',
        'author': '',
        'version': '',
        'permissions': [],
        'error': '',
    }


def failed(name, error):
    result = empty(name)
    result['error'] = error
    return result


def scan(path, name):
    flags = (os.O_RDONLY
             | getattr(os, 'O_NOFOLLOW', 0)
             | getattr(os, 'O_BINARY', 0))
    try:
        descriptor = os.open(path, flags)
    except OSError as error:
        return failed(name, 'Could not open file: %s' % error)
    try:
        info = os.fstat(descriptor)
        if not stat.S_ISREG(info.st_mode):
            return failed(name, 'Not a regular file.')
        if info.st_size > MAX_BYTES:
            return failed(name, 'File is larger than 2 MiB.')
        with os.fdopen(descriptor, 'rb') as source:
            descriptor = -1
            data = source.read(MAX_BYTES + 1)
    except OSError as error:
        return failed(name, 'Could not read file: %s' % error)
    finally:
        if descriptor >= 0:
            os.close(descriptor)
    if len(data) > MAX_BYTES:
        return failed(name, 'File is larger than 2 MiB.')
    try:
        text = data.decode('utf-8')
    except UnicodeDecodeError as error:
        return failed(name, 'Could not read file: %s' % error)
    try:
        tree = ast.parse(text, filename=name)
    except SyntaxError as error:
        return failed(name, 'Syntax error: %s' % (error.msg,))
    except ValueError as error:
        return failed(name, 'Syntax error: %s' % error)
    except RecursionError:
        return failed(name, 'File is nested too deeply to scan.')
    except MemoryError:
        return failed(name, 'File is too large to parse.')

    values = {}
    permissions = None
    for node in tree.body:
        if isinstance(node, ast.Assign) and len(node.targets) == 1:
            target = node.targets[0]
        elif isinstance(node, ast.AnnAssign):
            target = node.target
        else:
            continue
        if not isinstance(target, ast.Name):
            continue
        key = target.id
        if key not in STRINGS and key != '__permissions__':
            continue
        try:
            value = ast.literal_eval(node.value)
        except (ValueError, TypeError, SyntaxError, RecursionError):
            return failed(name, 'Metadata %s is not a literal.' % key)
        if key == '__permissions__':
            permissions = value
        else:
            values[STRINGS[key]] = value

    if 'id' not in values:
        return failed(name, 'Missing literal __id__.')
    if not isinstance(values['id'], str):
        return failed(name, 'Metadata __id__ is not a string.')
    if not ID_RE.match(values['id']):
        return failed(name, 'Invalid id: %s.' % values['id'])
    if 'name' not in values:
        return failed(name, 'Missing literal __name__.')
    if not isinstance(values['name'], str) or not values['name']:
        return failed(name, 'Metadata __name__ is not a non-empty string.')
    for key in ('description', 'author', 'version'):
        if not isinstance(values.get(key, ''), str):
            return failed(name, 'Metadata %s is not a string.' % key)
    if permissions is None:
        permissions = []
    if not isinstance(permissions, (list, tuple)):
        return failed(name, '__permissions__ is not a list of strings.')
    cleaned = []
    for permission in permissions:
        if not isinstance(permission, str):
            return failed(name, '__permissions__ is not a list of strings.')
        if permission not in KNOWN:
            return failed(name, 'Unknown permission: %s.' % permission)
        cleaned.append(permission)

    result = empty(name)
    result['id'] = values['id']
    result['name'] = values['name']
    result['description'] = values.get('description', '')
    result['author'] = values.get('author', '')
    result['version'] = values.get('version', '')
    result['permissions'] = cleaned
    return result


def main():
    directory = sys.argv[1]
    try:
        with os.scandir(directory) as iterator:
            names = []
            for entry in iterator:
                try:
                    info = entry.stat(follow_symlinks=False)
                except OSError:
                    continue
                if stat.S_ISREG(info.st_mode):
                    names.append(entry.name)
            names.sort()
    except OSError as error:
        print('Could not read directory: %s' % error, file=sys.stderr)
        sys.exit(1)

    results = []
    for name in names:
        lower = name.lower()
        if lower.endswith(UNSUPPORTED):
            results.append(failed(
                name,
                'Unsupported plugin format: %s.' % os.path.splitext(name)[1]))
        elif lower.endswith(SUPPORTED):
            results.append(scan(os.path.join(directory, name), name))
    print(json.dumps(results))


main()
)PY";

[[nodiscard]] bool ReadString(
		const QJsonObject &object,
		const char *key,
		QString *value) {
	if (!object.value(QLatin1String(key)).isString()) {
		return false;
	}
	*value = object.value(QLatin1String(key)).toString();
	return true;
}

[[nodiscard]] bool ParseEntry(const QJsonValue &value, Entry *entry) {
	if (!value.isObject()) {
		return false;
	}
	const auto object = value.toObject();
	if (!ReadString(object, "file", &entry->file)
		|| !ReadString(object, "id", &entry->id)
		|| !ReadString(object, "name", &entry->name)
		|| !ReadString(object, "description", &entry->description)
		|| !ReadString(object, "author", &entry->author)
		|| !ReadString(object, "version", &entry->version)
		|| !ReadString(object, "error", &entry->error)) {
		return false;
	}
	const auto permissions = object.value(QLatin1String("permissions"));
	if (!permissions.isArray()) {
		return false;
	}
	for (const auto &permission : permissions.toArray()) {
		if (!permission.isString()) {
			return false;
		}
		entry->permissions.push_back(permission.toString());
	}
	return true;
}

} // namespace

ScanResult ScanPlugins(const QString &directory) {
	auto result = ScanResult();
	const auto python = QStandardPaths::findExecutable(
		QStringLiteral("python3"));
	if (python.isEmpty()) {
		result.error = QStringLiteral("Python 3 was not found.");
		return result;
	}
	auto process = QProcess();
	process.start(python, {
		QStringLiteral("-I"),
		QStringLiteral("-B"),
		QStringLiteral("-c"),
		QString::fromUtf8(kScanScript),
		directory,
	});
	if (!process.waitForStarted(kStartTimeout)) {
		result.error = QStringLiteral("Plugin scan failed to start.");
		return result;
	}
	if (!process.waitForFinished(kScanTimeout)) {
		process.kill();
		process.waitForFinished(kKillTimeout);
		result.error = QStringLiteral("Plugin scan timed out.");
		return result;
	}
	const auto output = process.readAllStandardOutput();
	if (process.exitStatus() != QProcess::NormalExit
		|| process.exitCode() != 0) {
		const auto error = QString::fromUtf8(
			process.readAllStandardError()).trimmed();
		result.error = error.isEmpty()
			? QStringLiteral("Plugin scan failed.")
			: QStringLiteral("Plugin scan failed: %1").arg(error);
		return result;
	}
	auto parseError = QJsonParseError();
	const auto document = QJsonDocument::fromJson(output, &parseError);
	if (parseError.error != QJsonParseError::NoError || !document.isArray()) {
		result.error = QStringLiteral("Plugin scan returned invalid output.");
		return result;
	}
	const auto array = document.array();
	result.entries.reserve(array.size());
	for (const auto &value : array) {
		auto entry = Entry();
		if (!ParseEntry(value, &entry)) {
			result.entries.clear();
			result.error = QStringLiteral(
				"Plugin scan returned invalid output.");
			return result;
		}
		result.entries.push_back(std::move(entry));
	}
	return result;
}

} // namespace Exteraless::Plugins
