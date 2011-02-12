import re
import urllib
import itertools
import htmlentitydefs
import xml.parsers.expat

#app imports
import conf
import langcodes

LINKS = open('dumps/links','wb')
CATLINKS = open('dumps/catlinks','wb')
STRAYPREFIX = open('dumps/strayprefix','wb')
glob = {'countpages':0}

class DocState:
    """ Tracking state of XML document with expat parser """
    in_page= False
    
    in_text= False
    text= None

    in_title= False
    title= None
    count= 0

class WikiXmlParser(object):
    def __init__(self):
        self.state = DocState()

def start_element(name, attrs):
    """ Handle the start of XML tag """
    if name == u'page':
        assert(not DocState.in_page)
        DocState.in_page = True
        DocState.text = []
        DocState.title = []
    elif name == u'title':
        assert(DocState.in_page)
        DocState.in_title = True
    elif name == u'text':
        assert(DocState.in_page)
        DocState.in_text = True

def end_element(name):
    """ Handle the end of XML tag """
    if name == u'page':
        DocState.in_page = False
        process_page(DocState)
        glob['countpages'] += 1
        if (glob['countpages'] % 100) == 0:
            percent = glob['countpages']/41186.0
            print percent
            #if percent > 0.3: exit(0)
    elif name == u'title':
        assert(DocState.in_page)
        DocState.in_title = False
    elif name == u'text':
        assert(DocState.in_page)
        DocState.in_text = False

def char_data(data):
    """ Data in XML document """
    if DocState.in_title:
        assert(not DocState.in_text)
        DocState.title.append(data)
    elif DocState.in_text:
        assert(not DocState.in_title)
        DocState.text.append(data) #concat many strings is not a good idea

def process_page(doc):
    """ Complete wiki page, data is in doc.text and doc.title """
    text = ''.join(doc.text)
    title = ''.join(doc.title)

    assert(len(title)>0)
    assert(len(text)>1)

    #handle redirects
    if text[0:9].lower() == '#redirect':
        links = list(get_links(text));
        if len(links) == 1 and links[0][0] == 0:
            print links[0][1]
    else:
        for link_type, link in get_links(text):
            if link_type == 0:
                LINKS.write( (link+'\n').encode('utf-8') )
            elif link_type == 1:
                CATLINKS.write( (link+'\n').encode('utf-8') )
            else:
                raise Exception('Unknown link type')

##
# Parse links, too much magic going on here
# File mediawiki-1.16.1/includes/parser/Parser.php
# 1513:     function replaceInternalLinks2( &$s ) {
# useLinkPrefixExtension -> what?
def get_links(text):
    """ Return all wiki links from the given text """

    # Non-greedly match [[text]]
    # text cannot contain [ since nested [[ ]]'s indicate that from image 
    # description we are linked to another page. I am not interested in
    # image links, only page links and category links
    #
    for link in re.findall(r'\[\[([^\[]*?)\]\]', text):
        # Remove left blank space
        link = link.lstrip()
        # Potentially blank
        if len(link) == 0:
            continue
        # Strip off leading ':'
        escaped_link = link[0] == ':'
        if escaped_link:
            link = link[1:].lstrip()
        # Part of the link before the first | refers to the wiki page
        link = link.split('|')[0]
        # URL decode
        link = urllib.unquote(link)
        # Convert HTML entities into unicode
        link = unescape_html(link)
        # Everything after '#' is reference to section
        link = link.split('#')[0]
        # Underscores are like spaces
        link = link.replace('_',' ')
        # In wikipedia links, non-break space is considered regular space
        link = link.replace(unichr(160),' ')
        # Maybe someone put spaces arround the link text
        link = link.strip()
        # Link to section on the same page is not important
        if len(link) == 0:
            continue
        # Default link type is a link to page
        link_type = 0
        # Look for interwiki and links to other namespaces
        # Escaped links are treated as actual link
        if not escaped_link and ':' in link:
            # Split link in two parts
            prefix, afterprefix = link.split(':', 1)
            # I process prefix as lowerspaces
            prefix = prefix.rstrip().lower()
            # Skip interwiki links and images
            if prefix in langcodes.list or prefix in conf.NAMESPACES:
                continue # skip translations and other namespaces
            # Categories are case sensitive
            afterprefix = afterprefix.lstrip()
            if prefix == 'category' and afterprefix:
                # Link type is category
                link_type, link = 1, afterprefix
            else:
                STRAYPREFIX.write( (prefix+u'\n').encode('utf-8') )
        # Multiple spaces and underscores become single
        link = re.sub(' +',' ',link)
        yield link_type, link[0].upper() + link[1:]

    #Extra note:
    # Opening '[[' not followed by ']]' and vice versa 
    # can appear only for images and I don't care about images

##
# Removes HTML or XML character references and entities from a text string.
# from: http://effbot.org/zone/re-sub.htm#unescape-html
#
# @param text The HTML (or XML) source text.
# @return The plain text, as a Unicode string, if necessary.

def unescape_html(text):
    def fixup(m):
        text = m.group(0)
        if text[:2] == "&#":
            # character reference
            try:
                if text[:3] == "&#x":
                    return unichr(int(text[3:-1], 16))
                else:
                    return unichr(int(text[2:-1]))
            except ValueError:
                pass
        else:
            # named entity
            try:
                text = unichr(htmlentitydefs.name2codepoint[text[1:-1]])
            except KeyError:
                pass
        return text # leave as is
    return re.sub("&#?\w+;", fixup, text)

def test():
    text = u""" 
        [[LinkOne]] [[Sameline]]
        [[Link
        [[Link  _ 2#a|b]]
        [[Link__3#a|b]]
        [[Link___4#a|b]]
        [[Link____5#a|b]]
        [[smallcase]]
        [[   Link6    #sdffsf|asdfdsf]]
        [[Link%207]]
        [[Link&quot;]]
        [[Link&nbsp;8]]
        [[End test]]
    """
    for t, link in get_links(text):
        print '"'+link+'"'
    it = get_links(text)
    assert(it.next() == (0, 'LinkOne'))
    assert(it.next() == (0, 'Sameline'))
    assert(it.next() == (0, 'Link 2'))
    assert(it.next() == (0, 'Link 3'))
    assert(it.next() == (0, 'Link 4'))
    assert(it.next() == (0, 'Link 5'))
    assert(it.next() == (0, 'Smallcase'))
    assert(it.next() == (0, 'Link6'))
    assert(it.next() == (0, 'Link 7'))
    assert(it.next() == (0, 'Link"'))
    assert(it.next() == (0, 'Link 8'))
    assert(it.next() == (0, 'End test'))

class A():
    def start(self, name, attrs):


def main():
    #Some unit tests
    test()

    #Create Parser
    p = xml.parsers.expat.ParserCreate()

    p.StartElementHandler = start_element
    p.EndElementHandler = end_element
    p.CharacterDataHandler = char_data
    p.buffer_text = True

    for i in xrange(1,100):
        try:
            with open(conf.DUMPFILES % i, 'r') as f: p.ParseFile(f)
        except IOError:
            break #no more files

if __name__ == '__main__':
    main()


