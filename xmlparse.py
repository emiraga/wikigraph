import time, os, re
import urllib
import itertools
import htmlentitydefs
import xml.parsers.expat
import hashlib
import redis
import struct
import bz2

#app specific imports
import conf
import langcodes
from logger import setup_logger

REDIR_REGEX = re.compile(
            r'^\s*' #from beginning followed by space
            + '#redirect' # keyword
            + r'\s*:?\s*'  # space and potential colon
            + r'\[\[.+?\]\]', # regular link
            re.DOTALL | re.IGNORECASE )

NOWIKI_REGEX = re.compile(
              '<nowiki[^>]*>' # open tag
            + '.*?' # text between tags
            + '</nowiki[^>]*>', # close tag
            re.DOTALL | re.IGNORECASE )

LINKS_REGEX = re.compile(
              r'\[\[' # open [[
            + r'(.+?)' # non-greedily match the middle
            + r'\]\]' # close ]]
        )

def sha1(text):
    """ Compute hex sha1 of unicode string """
    return hashlib.sha1(text.encode('utf-8')).hexdigest()

class DocState:
    """ Tracking state of XML document with expat parser """
    in_page= False
    
    in_text= False
    text= None

    in_title= False
    title= None

class WikiXmlParser(object):
    """ Contains handlers for expat parser, sample usage:

            p = xml.parsers.expat.ParserCreate()
            w = WikiXmlParser()

            p.StartElementHandler = lambda name, attrs: \
                    w.start_element(name, attrs)
            p.EndElementHandler = lambda name: \
                    w.end_element(name)
            p.CharacterDataHandler = lambda data: \
                    w.char_data(data)

            w.set_page_callback(lambda doc: ... )
    """
    def __init__(self):
        self.state = DocState()
        self.count_pages = 0

    def set_page_callback(self, process_page_cb):
        self.process_page = process_page_cb

    def start_element(self, name, attrs):
        """ Handle the start of XML tag """
        if name == u'page':
            assert(not self.state.in_page)
            self.state.in_page = True
            self.state.text = []
            self.state.title = []
        elif name == u'title':
            assert(self.state.in_page)
            self.state.in_title = True
        elif name == u'text':
            assert(self.state.in_page)
            self.state.in_text = True

    def end_element(self, name):
        """ Handle the end of XML tag """
        if name == u'page':
            self.state.in_page = False
            self.process_page(self.state)
            self.count_pages += 1
            if (self.count_pages % 1000) == 0:
                print self.count_pages
        elif name == u'title':
            assert(self.state.in_page)
            self.state.in_title = False
        elif name == u'text':
            assert(self.state.in_page)
            self.state.in_text = False

    def char_data(self, data):
        """ Data in XML document """
        if self.state.in_title:
            assert(not self.state.in_text)
            self.state.title.append(data)
        elif self.state.in_text:
            assert(not self.state.in_title)
            self.state.text.append(data) #concat many strings is not a good idea

# Redirect magic
# File:mediawiki-1.16-1/includes/Title.php
#  365     protected static function newFromRedirectInternal( $text ) {
def process_page_stage1(doc, db):
    """ 
        Complete wiki page, data is in doc.text and doc.title 
        In step1 we only care about valid pages and redirects.
    """
    text = ''.join(doc.text)
    title = ''.join(doc.title)
    assert(len(title)>0)

    text = remove_nowiki(text)

    sha1_title = sha1(title)
    # Store title for easier lookup
    db.save_title(sha1_title, title)

    redirect = get_redirect(text)
    if redirect:
        db.set_redirect(sha1_title, sha1(redirect))
    else:
        db.set_page_id(sha1_title, db.incr('next:page_id'))

def write_edges_file(node_id, links, fout):
    """ Write edge list to file in binary format """
    fout.write(struct.pack('i',int(node_id)))
    fout.write(struct.pack('i',len(links)))
    for link in links:
        fout.write(struct.pack('i',int(link)))

def process_page_stage3(doc, f_pages, f_cats, db):
    """ Complete wiki page, data is in doc.text and doc.title 
    """
    text = ''.join(doc.text)
    title = ''.join(doc.title)

    text = remove_nowiki(text)

    assert(len(title)>0)

    sha1_title = sha1(title)

    redirect = get_redirect(text)
    if redirect:
        pass
    else:
        page_id = db.get_page_id(sha1_title)
        assert(page_id != None)

        page_links = []
        cat_links = []
        for link_type, link in get_links(text):
            if link_type == 0:
                # Find id of the linked page
                sha1_link = sha1(link)
                link_id = db.get_page_id(sha1_link)
                if link_id:
                    page_links.append(link_id)
            elif link_type == 1:
                cat_id = db.get_category_id(link, sha1(link))
                cat_links.append(cat_id)
                # Add this page set to category
                db.add_page_to_cat(cat_id, page_id)
            else:
                raise Exception('Unknown link type')
        write_edges_file(page_id, page_links, f_pages)
        write_edges_file(page_id, cat_links, f_cats)

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
    for link in LINKS_REGEX.findall(text):
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
                pass # STRAYPREFIX.write( (prefix+u'\n').encode('utf-8') )
        # Multiple spaces and underscores become single
        link = re.sub(' +',' ',link)
        yield link_type, link[0].upper() + link[1:]

    #Extra note:
    # Opening '[[' not followed by ']]' and vice versa 
    # can appear only for images and I don't care about images

def get_redirect(text):
    redir = REDIR_REGEX.match(text)
    if redir:
        # reuse code from get_links since link handling is complex
        links = list(get_links(redir.group()))
        assert(len(links) <= 1)
        # Take redirects to regular page only
        if len(links) and links[0][0] == 0:
            return links[0][1]

def remove_nowiki(text):
    return NOWIKI_REGEX.sub('', text)


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

def get_dump_files():
    """ Based on conf.py generate list of all dump files """
    for file_name in os.listdir('dumps'):
        if file_name.startswith(conf.DUMPFILES):
            yield 'dumps/'+file_name

class MyRedis(object):
    """ Trivial Redis db used for testing
    Don't use it, does not save keys persistently """
    def __init__(self):
        self.m = {}
    def set(self, name, value):
        self.m[name] = value
    def get(self, name):
        return str(self.m[name])
    def incr(self, name):
        self.m[name] += 1
        return str(self.m[name])
    def setnx(self, name, value):
        if name in self.m:
            return 0
        else:
            self.set(name, value)
            return 1
    def sadd(self, set_, element):
        pass
    def keys(self, pattern):
        return []

def get_parsers():
    """ Creates a pair of parsers """
    parser = xml.parsers.expat.ParserCreate()
    wiki_parse = WikiXmlParser()

    # Configure
    parser.buffer_text = True
    parser.StartElementHandler = lambda name, attrs: \
            wiki_parse.start_element(name, attrs)
    parser.EndElementHandler = lambda name: \
            wiki_parse.end_element(name)
    parser.CharacterDataHandler = lambda data: \
            wiki_parse.char_data(data)
    return parser, wiki_parse

class Database(object):
    """ Persistence models
    
    For certain operations redis will be used,
    whereas for others append-to files
    """
    def __init__(self, config):
        self.redis = redis.Redis(
            host=config.REDIS_HOST, 
            port=config.REDIS_PORT,
            db=config.REDIS_DB)
        # For mapping hash names to titles regular file is used
        self.names = open('hash_names','w')
        # For a link between category and page also file
        self.cat_page = open('category_page','w')
        # Use regular python logging facility
        self.logger = setup_logger()

    def close(self):
        self.names.close()
        self.cat_page.close()

    def log_info(self, msg):
        self.logger.info(msg)
    
    def incr(self, key):
        return self.redis.incr(key)

    def get(self, key):
        return self.redis.get(key)

    def set(self, key, value):
        return self.redis.set(key, value)

    def keys(self, pattern):
        return self.redis.keys(pattern=pattern)

    def hkeys(self, key):
        return self.redis.hkeys(key)

    def save_title(self, h_title, value):
        """ Save the hash to value mapping 
            Duplication is possible.  """
        self.names.write((h_title+value+'\n').encode('utf-8'))

    def set_redirect(self, h_title, h_redirect):
        return self.redis.hset('redirect:' + h_title[:20], h_title[20:],
            h_redirect)

    def set_page_id(self, h_title, val):
        return self.redis.hset('page:'+h_title[:20], h_title[20:], val)

    def get_page_id(self, h_title):
        return self.redis.hget('page:'+h_title[:20], h_title[20:])

    def resolve_redirect(self, key, visited):
        redirpage = key.split(':')[1]
        redirpage1, redirpage2 = redirpage[:20], redirpage[20:]

        if self.redis.hexists('page:'+redirpage1, redirpage2):
            return # already resolved
        target = self.redis.hget('redirect:'+redirpage1, redirpage2)
        if target == None:
            return # it's not there

        if target in visited:
            self.log_info('Found a redirect loop involving: '+str(visited))
            return
        visited.add(target)

        # In case of multiple redirects resolve them recursively
        self.resolve_redirect('redirect:'+target, visited)
        
        # Try to get page_id directly
        page_id = self.redis.hget('page:'+target[:20], target[20:])
        if page_id != None:
            # Set proper page_id and delete redirect
            self.redis.hset('page:'+redirpage1, redirpage2, page_id)
            self.redis.hdel('redirect:'+redirpage1, redirpage2)
            return

    def get_category_id(self, name, h_name):
        """ Return a category_id from name of category,
            or else new category id will be assigned
        """
        h_name1, h_name2 = h_name[:20], h_name[20:]
        cat_id = self.redis.hget('category:'+h_name1, h_name2)
        if cat_id:
            return cat_id
        # Titles are shared between pages and categories
        self.save_title(h_name, name)
        # One problem here is that sometimes category ID's will be unused
        cat_id = self.redis.incr('next:category_id')
        # Check if category already exists
        if self.redis.hsetnx('category:'+h_name1, h_name2, cat_id):
            # This is the first time we see this category
            return cat_id
        else:
            # Someone else did set this value, yikes. Record unused ID.
            # If this client dies unused cat ID will not be recorded.
            self.redis.sadd('unused_category_ids',cat_id)
            # Return actual ID
            return self.redis.hget('category:'+h_name1, h_name2)
    
    def add_page_to_cat(self, cat_id, page_id):
        self.cat_page.write(struct.pack('i',int(cat_id)))
        self.cat_page.write(struct.pack('i',int(page_id)))

def main():
    # Connect to redis
    db = Database(conf)

    if True:#Stage 1
        t0 = time.time()
        db.log_info('Starting stage 1')
        main_stage1(db)
        db.log_info('Stage 1, '+str(time.time()-t0)+" seconds processing time")
        db.log_info('Unique pages:'+ db.get('next:page_id') )

    if True:#Stage 2
        t0 = time.time()
        db.log_info('Starting stage 2')
        main_stage2(db)
        db.log_info('Stage 2, '+str(time.time()-t0)+" seconds processing time")
        db.log_info('Unresolved redirects:' +
                str(len(db.keys(pattern = 'redirect:*'))))

    if True:#Stage 3
        t0 = time.time()
        db.log_info('Starting stage 3')
        main_stage3(db)
        db.log_info('Stage 3, '+str(time.time()-t0)+" seconds processing time")
        db.log_info('Categories:'+ db.get('next:category_id') )

    db.close()

def main_stage1(db):
    # Reset counters to zero
    db.set('next:page_id', 0)
    db.set('next:category_id', 0)
 
    for file_name in get_dump_files():
        db.log_info('Processing file' + file_name)
        # Create Parsers
        parser, wiki_parse = get_parsers()
        # First stage is only geting a list of valid pages and redirects
        wiki_parse.set_page_callback(lambda doc: 
                process_page_stage1(doc, db))
        # Plain text XML files
        if file_name.endswith('.xml'):
            with open(file_name, 'rb') as f:
                parser.ParseFile(f)
        # Compressed XML with bz2
        elif file_name.endswith('.xml.bz2'):
            f = bz2.BZ2File(file_name, 'rb')
            parser.ParseFile(f)
            f.close()

def main_stage2(db):
    # Resolve redirects
    for redirect1 in db.keys(pattern = 'redirect:*'):
        for redirect2 in db.hkeys(redirect1):
            db.resolve_redirect(redirect1 + redirect2, set())

def main_stage3(db):
    with open('graph_cat.bin','wb') as f_cats:
        with open('graph_pages.bin','wb') as f_pages:
            for file_name in get_dump_files():
                db.log_info( 'Processing file'+ file_name )
                # Create Parsers
                parser, wiki_parse = get_parsers()
                # Third stage is for finding links
                wiki_parse.set_page_callback(lambda doc:
                    process_page_stage3(doc, f_pages, f_cats, db))
                # Plain text XML files
                if file_name.endswith('.xml'):
                    with open(file_name, 'rb') as f:
                        parser.ParseFile(f)
                # Compressed XML with bz2
                elif file_name.endswith('.xml.bz2'):
                    f = bz2.BZ2File(file_name, 'rb')
                    parser.ParseFile(f)
                    f.close()

def test():
    """ Run couple of tests, not comprehensive """
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
        [[Uniform polyexon#The E7 [33,2,1] family (3 21)]]
        [[Uniform polyexon [33,2,1] family (3 21)]]
        <nowiki>[[Nolink]]</nowiki> < nowiki>[[Yeslink]]</nowiki>
        <code>[[Link 9]]</code>
        [[File:A.jpg| [[No Match]]    //this is not matched
        [[File:A.jpg| [[No Match]] ]] //this is not matched
        [[End test]]
    """
    out = [
        (0, 'LinkOne'),
        (0, 'Sameline'),
        (0, 'Link 2'),
        (0, 'Link 3'),
        (0, 'Link 4'),
        (0, 'Link 5'),
        (0, 'Smallcase'),
        (0, 'Link6'),
        (0, 'Link 7'),
        (0, 'Link"'),
        (0, 'Link 8'),
        (0, 'Uniform polyexon'),
        (0, 'Uniform polyexon [33,2,1] family (3 21)'),
        (0, 'Yeslink'),
        (0, 'Link 9'),
        (0, 'End test')
    ]
    text = remove_nowiki(text)
    #for t, link in get_links(text): print '"'+link+'"'
    it = get_links(text)
    for expected in out:
        assert(it.next() == expected)

    text = """
        #reDirEct [[hahah[] 2]] [[adsasd]]
    """
    assert( get_redirect(text) == 'Hahah[] 2' )

    text = """<code>#redirect [[Link]]</code>"""
    assert( get_redirect(text) == None )

    text = """.#redirect [[Link]]</code>"""
    assert( get_redirect(text) == None )

if __name__ == '__main__':
    test()
    main()

