<?php

/**
 * Simple Feed Aggregator
 * $Id$
 * 
 * @copyright   Michael Wallner, <mike@iworks.at>
 * @license     BSD, revised
 * @package     pecl/http
 * @version     $Revision$
 */
class FeedAggregator
{
    /**
     * Cache directory
     *
     * @var string
     */
    public $directory;
    
    /**
     * Feeds
     *
     * @var array
     */
    protected $feeds = array();

    /**
     * Constructor
     *
     * @param string $directory
     */
    public function __construct($directory = 'feeds')
    {
        $this->setDirectory($directory);
    }

    /**
     * Set cache directory
     *
     * @param string $directory
     */
    public function setDirectory($directory)
    {
        $this->directory = $directory;
        foreach (glob($this->directory .'/*.xml') as $feed) {
            $this->feeds[basename($feed, '.xml')] = filemtime($feed);
        }
    }

    /**
     * Strips all special chars
     *
     * @param string $url
     * @return string
     */
    public function url2name($url)
    {
        return preg_replace('/[^\w\.-]+/', '_', $url);
    }
    
    /**
     * Checks if $url is a known feed
     *
     * @param string $url
     * @return bool
     */
    public function hasFeed($url)
    {
        return isset($this->feeds[$this->url2name($url)]);
    }

    /**
     * Add an URL as feed
     *
     * @param string $url
     * @return void
     * @throws Exception
     */
    public function addFeed($url)
    {
        $r = $this->setupRequest($url);
        $r->send();
        $this->handleResponse($r);
    }

    /**
     * Add several URLs as feeds
     *
     * @param array $urls
     * @return void
     * @throws Exception
     */
    public function addFeeds(array $urls)
    {
        $pool = new HttpRequestPool;
        foreach ($urls as $url) {
            $pool->attach($r = $this->setupRequest($url));
        }
        $pool->send();

        foreach ($pool as $request) {
            $this->handleResponse($request);
        }
    }

    /**
     * Load a feed (from cache)
     *
     * @param string $url
     * @return string
     * @throws Exception
     */
    public function getFeed($url)
    {
        $this->addFeed($url);
        return $this->loadFeed($this->url2name($url));
    }

    /**
     * Load several feeds (from cache)
     *
     * @param array $urls
     * @return array
     * @throws Exception
     */
    public function getFeeds(array $urls)
    {
        $feeds = array();
        $this->addFeeds($urls);
        foreach ($urls as $url) {
            $feeds[] = $this->loadFeed($this->url2name($url));
        }
        return $feeds;
    }

    protected function saveFeed($file, $contents)
    {
        if (file_put_contents($this->directory .'/'. $file .'.xml', $contents)) {
            $this->feeds[$file] = time();
        } else {
            throw new Exception("Could not save feed contents to $file.xml");
        }
    }

    protected function loadFeed($file)
    {
        if (isset($this->feeds[$file])) {
            if ($data = file_get_contents($this->directory .'/'. $file .'.xml')) {
                return $data;
            } else {
                throw new Exception("Could not load feed contents from $file.xml");
            }
        } else {
            throw new Exception("Unknown feed/file $file.xml");
        }
    }

    protected function setupRequest($url, $escape = true)
    {
        $r = new HttpRequest($url);
        $r->setOptions(array('redirect' => true));

        $file = $escape ? $this->url2name($url) : $url;

        if (isset($this->feeds[$file])) {
            $r->setOptions(array('lastmodified' => $this->feeds[$file]));
        }

        return $r;
    }

    protected function handleResponse(HttpRequest $r)
    {
        if ($r->getResponseCode() != 304) {
            if ($r->getResponseCode() != 200) {
                throw new Exception("Unexpected response code ". $r->getResponseCode());
            }
            if (!strlen($body = $r->getResponseBody())) {
                throw new Exception("Received empty feed from ". $r->getUrl());
            }
            $this->saveFeed($this->url2name($r->getUrl()), $body);
        }
    }
}

?>
