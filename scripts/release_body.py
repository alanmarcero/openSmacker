"""Build a GitHub release body for one tag.

Notes come from a matching section in CHANGELOG.md or CHANGELOG. A heading
matches when it contains the tag, or the same version without a leading v,
as its own token (`## v2.7.0`, `## [2.7.0] - date`). The section runs until
the next heading of the same or higher level.

When no section has any text, the notes are the annotated tag message.
A lightweight tag, or an annotated tag with an empty message, uses the
tagged commit message instead. Git *-by trailers are left out.

The screenshot and compare link are always added after those notes.
"""

from pathlib import Path
import argparse
import re
import subprocess
import sys

TRAILER = re.compile(r"^[A-Za-z0-9][A-Za-z0-9-]*-by: ")
HEADING = re.compile(r"^(#{1,6})\s+(.*\S)\s*$")
CHANGELOG_NAMES = ("CHANGELOG.md", "CHANGELOG")


def git(root, *args):
    result = subprocess.run(
        ["git", *args],
        cwd=root,
        text=True,
        capture_output=True,
        check=False,
    )
    if result.returncode != 0:
        detail = result.stderr.strip() or result.stdout.strip() or "git failed"
        raise RuntimeError(detail)
    return result.stdout


def heading_matches(title, tag):
    candidates = [tag]
    if len(tag) > 1 and tag[0] in "vV" and tag[1].isdigit():
        candidates.append(tag[1:])
    for candidate in candidates:
        if re.search(rf"(?<![\w.]){re.escape(candidate)}(?![\w.])", title):
            return True
    return False


def section_body(text, tag):
    lines = text.splitlines()
    start = None
    level = None
    for index, line in enumerate(lines):
        heading = HEADING.match(line)
        if heading is None:
            continue
        if start is None:
            if heading_matches(heading.group(2), tag):
                start = index + 1
                level = len(heading.group(1))
            continue
        if len(heading.group(1)) <= level:
            return "\n".join(lines[start:index]).strip()
    if start is None:
        return ""
    return "\n".join(lines[start:]).strip()


def changelog_notes(root, tag):
    for name in CHANGELOG_NAMES:
        path = root / name
        if not path.is_file():
            continue
        notes = section_body(path.read_text(encoding="utf-8"), tag)
        if notes:
            return notes
    return ""


def clean_message(message):
    lines = []
    for line in message.splitlines():
        if line.startswith("-----BEGIN ") and "SIGNATURE" in line:
            break
        lines.append(line)
    while lines and not lines[-1].strip():
        lines.pop()
    while lines and TRAILER.match(lines[-1]):
        lines.pop()
        while lines and not lines[-1].strip():
            lines.pop()
    return "\n".join(lines).strip()


def tag_message(root, tag):
    message = clean_message(git(root, "for-each-ref", f"refs/tags/{tag}", "--format=%(contents)"))
    if message:
        return message
    return clean_message(git(root, "log", "-1", "--format=%B", tag))


def previous_tag(root, tag):
    result = subprocess.run(
        ["git", "describe", "--tags", "--abbrev=0", f"{tag}^"],
        cwd=root,
        text=True,
        capture_output=True,
        check=False,
    )
    if result.returncode != 0:
        return ""
    return result.stdout.strip()


def release_body(notes, repo, tag, previous):
    parts = [notes.strip()]
    parts.append(
        f"![openSmacker native editor](https://raw.githubusercontent.com/{repo}/{tag}/docs/screenshots/preview.png)"
    )
    if previous:
        parts.append(f"**Full Changelog**: https://github.com/{repo}/compare/{previous}...{tag}")
    return "\n\n".join(parts) + "\n"


def build(root, repo, tag):
    notes = changelog_notes(root, tag) or tag_message(root, tag)
    if not notes:
        raise RuntimeError(f"no CHANGELOG section or tag message for {tag}")
    return release_body(notes, repo, tag, previous_tag(root, tag))


def parse_args(argv):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--tag", required=True)
    parser.add_argument("--repo", required=True)
    parser.add_argument("--output", required=True)
    parser.add_argument("--root", default=".")
    return parser.parse_args(argv)


def main(argv):
    args = parse_args(argv)
    if not re.fullmatch(r"[\w.-]+/[\w.-]+", args.repo):
        raise SystemExit(f"unexpected repository name: {args.repo}")
    if not re.fullmatch(r"[\w.+-]+", args.tag):
        raise SystemExit(f"unexpected tag name: {args.tag}")
    try:
        body = build(Path(args.root).resolve(), args.repo, args.tag)
    except RuntimeError as error:
        raise SystemExit(str(error)) from error
    Path(args.output).write_text(body, encoding="utf-8")


if __name__ == "__main__":
    main(sys.argv[1:])
