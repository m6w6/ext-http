<?php

/**
 * BigGet - download big files efficiently
 * $Id$
 * 
 * @copyright   Michael Wallner, <mike@iworks.at>
 * @license     BSD, revised
 * @version     $Revision$
 */
class BigGet extends HttpRequestPool
{
    /**
     * File split size
     */
    const SIZE = 1048576;
    
    /**
     * Parallel Request count
     */
    const RMAX = 5;
    
    /**
     * Whether to output debug messages
     * 
     * @var bool
     */
    public $dbg = false;
    
    /**
     * URL
     *
     * @var string
     */
    private $url;
    
    /**
     * Temp file prefix
     * 
     * @var string
     */
    private $tmp;
    
    /**
     * Size of requested resource
     *
     * @var int
     */
    private $size;
    
    /**
     * Whether the requests have been sent
     *
     * @var bool
     */
    private $sent = false;
    
    /**
     * Request counter
     *
     * @var int
     */
    private $count = 0;
    
    /**
     * Static constructor
     *
     * @param string $url
     * @param string $tmp
     * @return BigGet
     * @throws Exception
     */
    public static function url($url, $tmp = '/tmp')
    {
        $head = new HttpRequest($url, HttpRequest::METH_HEAD);
        $headers = $head->send()->getHeaders();
        
        if (200 != $head->getResponseCode()) {
            throw new HttpException("Did not receive '200 Ok' from HEAD $url");
        }
        if (!isset($headers['Accept-Ranges'])) {
            throw new HttpException("Did not receive an Accept-Ranges header from HEAD $url");
        }
        if (!isset($headers['Content-Length'])) {
            throw new HttpException("Did not receive a Content-Length header from HEAD $url");
        }
        
        $bigget = new BigGet;
        $bigget->url = $url;
        $bigget->tmp = tempnam($tmp, 'BigGet.');
        $bigget->size = $headers['Content-Length'];
        return $bigget;
    }
    
    /**
     * Save the resource to a file
     *
     * @param string $file
     * @return bool
     * @throws Exception
     */
    public function saveTo($file)
    {
        $this->sent or $this->send();
        
        if ($w = fopen($this->tmp, 'wb')) {
            
            $this->dbg && print "\nCopying temp files to $file ...\n";
            
            foreach (glob($this->tmp .".????") as $tmp) {
                
                $this->dbg && print "\t$tmp\n";
                
                if ($r = fopen($tmp, 'rb')) {
                    stream_copy_to_stream($r, $w);
                    fclose($r);
                }
                unlink($tmp);
            }
            fclose($w);
            rename($this->tmp, $file);
            
            $this->dbg && print "\nDone.\n";
            
            return true;
        }
        return false;
    }
    
    /**
     * Overrides HttpRequestPool::send()
     *
     * @return void
     * @throws Exception
     */
    public function send()
    {
        $this->sent = true;
        
        // use max RMAX simultanous requests with a req size of SIZE
        while ($this->count < self::RMAX && -1 != $offset = $this->getRangeOffset()) {
            $this->attachNew($offset);
        }
        
        while ($this->socketPerform()) {
            if (!$this->socketSelect()) {
                throw new HttpSocketException;
            }
        }
    }
    
    /**
     * Overrides HttpRequestPool::socketPerform()
     *
     * @return bool
     */
    protected function socketPerform()
    {
        $rs = parent::socketPerform();
        
        foreach ($this->getFinishedRequests() as $r) {
            $this->detach($r);
            
            if (206 != $rc = $r->getResponseCode()) {
                throw new HttpException("Unexpected response code: $rc");
            }
            
            file_put_contents(sprintf("%s.%04d", $this->tmp, $r->id), $r->getResponseBody());
            
            if (-1 != $offset = $this->getRangeOffset()) {
                $this->attachNew($offset);
            }
        }
        
        return $rs;
    }
    
    private function attachNew($offset)
    {
        $stop = min($this->count * self::SIZE + self::SIZE, $this->size) - 1;
        
        $this->dbg && print "Attaching new request to get range: $offset-$stop\n";
        
        $req = new BigGetRequest(
            $this->url,
            HttpRequest::METH_GET,
            array(
                'headers' => array(
                    'Range' => "bytes=$offset-$stop"
                )
            )
        );
        $this->attach($req);
        $req->id = $this->count++;
    }
    
    private function getRangeOffset()
    {
        return ($this->size >= $start = $this->count * self::SIZE) ? $start : -1;
    }
}


/**
 * BigGet request
 * @ignore
 */
class BigGetRequest extends HttpRequest
{
    public $id;
}

?>
