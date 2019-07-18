// Scintilla source code edit control
/** @file WordList.cxx
 ** Hold a list of words.
 **/
// Copyright 1998-2002 by Neil Hodgson <neilh@scintilla.org>
// The License.txt file describes the conditions under which this software may be distributed.

#include <cstdlib>
#include <cassert>
#include <cstring>

#include <algorithm>
#include <iterator>

#include "WordList.h"

using namespace Scintilla;

/**
 * Creates an array that points into each word in the string and puts \0 terminators
 * after each word.
 */
static char **ArrayFromWordList(char *wordlist, size_t slen, int *len, bool onlyLineEnds = false) {
	int prev = true;
	int words = 0;
	// For rapid determination of whether a character is a separator, build
	// a look up table.
	bool wordSeparator[256] = {};	// Initialise all to false.
	wordSeparator[static_cast<unsigned int>('\r')] = true;
	wordSeparator[static_cast<unsigned int>('\n')] = true;
	if (!onlyLineEnds) {
		wordSeparator[static_cast<unsigned int>(' ')] = true;
		wordSeparator[static_cast<unsigned int>('\t')] = true;
	}

	char * const end = wordlist + slen;
	char *s = wordlist;
	while (s < end) {
		const unsigned char ch = *s++;
		const bool curr = wordSeparator[ch];
		if (!curr && prev) {
			words++;
		}
		prev = curr;
	}

	char **keywords = new char *[words + 1];
	int wordsStore = 0;
	if (words) {
		prev = '\0';
		s = wordlist;
		while (s < end) {
			unsigned char ch = *s;
			if (!wordSeparator[ch]) {
				if (!prev) {
					keywords[wordsStore] = s;
					wordsStore++;
				}
			} else {
				*s = '\0';
				ch = '\0';
			}
			prev = ch;
			++s;
		}
	}

	assert(wordsStore < (words + 1));
	keywords[wordsStore] = end;
	*len = wordsStore;
	return keywords;
}

WordList::WordList(bool onlyLineEnds_) noexcept :
	words(nullptr), list(nullptr), len(0), onlyLineEnds(onlyLineEnds_) {
	// Prevent warnings by static analyzers about uninitialized starts.
	starts[0] = -1;
}

WordList::~WordList() {
	Clear();
}

WordList::operator bool() const noexcept {
	return len != 0;
}

bool WordList::operator!=(const WordList &other) const noexcept {
	if (len != other.len)
		return true;
	for (int i = 0; i < len; i++) {
		if (strcmp(words[i], other.words[i]) != 0)
			return true;
	}
	return false;
}

int WordList::Length() const noexcept {
	return len;
}

void WordList::Clear() noexcept {
	if (words) {
		delete[]list;
		delete[]words;
	}
	words = nullptr;
	list = nullptr;
	len = 0;
}

void WordList::Set(const char *s) {
	Clear();
	const size_t lenS = strlen(s) + 1;
	list = new char[lenS];
	memcpy(list, s, lenS);
	words = ArrayFromWordList(list, lenS - 1, &len, onlyLineEnds);
	std::sort(words, words + len, [](const char *a, const char *b) noexcept {
		return strcmp(a, b) < 0;
	});
	std::fill(starts, std::end(starts), -1);
	for (int l = len - 1; l >= 0; l--) {
		unsigned char indexChar = words[l][0];
		starts[indexChar] = l;
	}
}

bool WordList::Reset(const char *s) {
#if 0
	WordList other;
	other.Set(s);
	if (*this == other) {
		return false;
	}

	Clear();
	words = other.words;
	list = other.list;
	len = other.len;
	onlyLineEnds = other.onlyLineEnds;
	memcpy(starts, other.starts, sizeof(starts));
	// mark other as released.
	other.words = nullptr;
	other.list = nullptr;
	other.len = 0;
#else
	Set(s);
#endif
	return true;
}

/** Check whether a string is in the list.
 * List elements are either exact matches or prefixes.
 * Prefix elements start with '^' and match all strings that start with the rest of the element
 * so '^GTK_' matches 'GTK_X', 'GTK_MAJOR_VERSION', and 'GTK_'.
 */
bool WordList::InList(const char *s) const noexcept {
	if (nullptr == words)
		return false;
	const unsigned char firstChar = s[0];
	int j = starts[firstChar];
	if (j >= 0) {
		while (words[j][0] == firstChar) {
			if (s[1] == words[j][1]) {
				const char *a = words[j] + 1;
				const char *b = s + 1;
				while (*a && *a == *b) {
					a++;
					b++;
				}
				if (!*a && !*b)
					return true;
			}
			j++;
		}
	}
	j = starts[static_cast<unsigned int>('^')];
	if (j >= 0) {
		while (words[j][0] == '^') {
			const char *a = words[j] + 1;
			const char *b = s;
			while (*a && *a == *b) {
				a++;
				b++;
			}
			if (!*a)
				return true;
			j++;
		}
	}
	return false;
}

/** similar to InList, but word s can be a substring of keyword.
 * eg. the keyword define is defined as def~ine. This means the word must start
 * with def to be a keyword, but also defi, defin and define are valid.
 * The marker is ~ in this case.
 */
bool WordList::InListAbbreviated(const char *s, const char marker) const noexcept {
	if (nullptr == words)
		return false;
	const unsigned char firstChar = s[0];
	int j = starts[firstChar];
	if (j >= 0) {
		while (words[j][0] == firstChar) {
			bool isSubword = false;
			int start = 1;
			if (words[j][1] == marker) {
				isSubword = true;
				start++;
			}
			if (s[1] == words[j][start]) {
				const char *a = words[j] + start;
				const char *b = s + 1;
				while (*a && *a == *b) {
					a++;
					if (*a == marker) {
						isSubword = true;
						a++;
					}
					b++;
				}
				if ((!*a || isSubword) && !*b)
					return true;
			}
			j++;
		}
	}
	j = starts[static_cast<unsigned int>('^')];
	if (j >= 0) {
		while (words[j][0] == '^') {
			const char *a = words[j] + 1;
			const char *b = s;
			while (*a && *a == *b) {
				a++;
				b++;
			}
			if (!*a)
				return true;
			j++;
		}
	}
	return false;
}

/** similar to InListAbbreviated, but word s can be a abridged version of a keyword.
* eg. the keyword is defined as "after.~:". This means the word must have a prefix (begins with) of
* "after." and suffix (ends with) of ":" to be a keyword, Hence "after.field:" , "after.form.item:" are valid.
* Similarly "~.is.valid" keyword is suffix only... hence "field.is.valid" , "form.is.valid" are valid.
* The marker is ~ in this case.
* No multiple markers check is done and wont work.
*/
bool WordList::InListAbridged(const char *s, const char marker) const noexcept {
	if (nullptr == words)
		return false;
	const unsigned char firstChar = s[0];
	int j = starts[firstChar];
	if (j >= 0) {
		while (words[j][0] == firstChar) {
			const char *a = words[j];
			const char *b = s;
			while (*a && *a == *b) {
				a++;
				if (*a == marker) {
					a++;
					const size_t suffixLengthA = strlen(a);
					const size_t suffixLengthB = strlen(b);
					if (suffixLengthA >= suffixLengthB)
						break;
					b = b + suffixLengthB - suffixLengthA - 1;
				}
				b++;
			}
			if (!*a && !*b)
				return true;
			j++;
		}
	}

	j = starts[static_cast<unsigned int>(marker)];
	if (j >= 0) {
		while (words[j][0] == marker) {
			const char *a = words[j] + 1;
			const char *b = s;
			const size_t suffixLengthA = strlen(a);
			const size_t suffixLengthB = strlen(b);
			if (suffixLengthA > suffixLengthB) {
				j++;
				continue;
			}
			b = b + suffixLengthB - suffixLengthA;

			while (*a && *a == *b) {
				a++;
				b++;
			}
			if (!*a && !*b)
				return true;
			j++;
		}
	}

	return false;
}

const char *WordList::WordAt(int n) const noexcept {
	return words[n];
}

