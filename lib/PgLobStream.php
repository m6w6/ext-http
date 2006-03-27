<?php

/**
 * PostgreSQL LOB stream
 * $Id$
 * 
 * Usage:
 * <code>
 * // GET /image.php?image=1234
 * if (PgLobStream::$loId = (int) $_GET['image']) {
 *     if ($lob = fopen('pglob://dbname=database user=mike', 'r')) {
 *         HttpResponse::setContentType('image/jpeg');
 *         HttpResponse::setStream($lob);
 *         HttpResponse::send();
 *     }
 * }
 * </code>
 * 
 * @copyright   Michael Wallner, <mike@iworks.at>
 * @license     BSD, revised
 * @package     pecl/http
 * @version     $Revision$
 */
class PgLobStream
{
    private $dbh;
    private $loh;
    private $lon;
    private $size = 0;
    
    public static $loId;

    function stream_open($path, $mode)
    {
        $path = trim(parse_url($path, PHP_URL_HOST));
        
        if ($path) {
            if ($this->dbh = pg_connect($path)) {
                if (pg_query($this->dbh, 'BEGIN')) {
                    if (is_resource($this->loh = pg_lo_open($this->dbh, $this->lon = self::$loId, $mode))) {
                        pg_lo_seek($this->loh, 0, PGSQL_SEEK_END);
                        $this->size = (int) pg_lo_tell($this->loh);
                        pg_lo_seek($this->loh, 0, PGSQL_SEEK_SET);
                        return true;
                    }
                }
            }
        }
        return false;
    }
    
    function stream_read($length)
    {
        return pg_lo_read($this->loh, $length);
    }
    
    function stream_seek($offset, $whence = PGSQL_SEEK_SET)
    {
        return pg_lo_seek($this->loh, $offset, $whence);
    }
    
    function stream_tell()
    {
        return pg_lo_tell($this->loh);
    }
    
    function stream_eof()
    {
        return pg_lo_tell($this->loh) >= $this->size;
    }
    
    function stream_flush()
    {
        return true;
    }
    
    function stream_stat()
    {
        return array('size' => $this->size, 'ino' => $this->lon);
    }
    
    function stream_write($data)
    {
        return pg_lo_write($this->loh, $data);
    }
    
    function stream_close()
    {
        if (pg_lo_close($this->loh)) {
            return pg_query($this->dbh, 'COMMIT');
        } else {
            pg_query($this->dbh, 'ROLLBACK');
            return false;
        }
    }
}

stream_register_wrapper('pglob', 'PgLobStream');

?>
