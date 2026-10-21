import sys


def main() -> int:
    for line in sys.stdin:
        s = line.strip()
        if not s:
            continue
        if s.startswith(">"):
            s = s[1:].strip()
            if not s:
                continue
        print(s)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

