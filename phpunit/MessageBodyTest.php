<?php

class MessageBodyTest extends PHPUnit_Framework_TestCase {
    protected $file, $temp;

    function setUp() {
        $this->file = new http\Message\Body(fopen(__FILE__, "r"));
        $this->temp = new http\Message\Body();
    }

    function testStat() {
        $this->assertEquals(filesize(__FILE__), $this->file->stat("size"));
        $this->assertEquals(filemtime(__FILE__), $this->file->stat("mtime"));
        $this->assertEquals(fileatime(__FILE__), $this->file->stat("atime"));
        $this->assertEquals(filectime(__FILE__), $this->file->stat("ctime"));
        $this->assertEquals(
            array(
                "size" => 0,
                "mtime" => 0,
                "atime" => 0,
                "ctime" => 0,
            ),
            $this->temp->stat()
        );
    }

    function testAppend() {
        $this->assertEquals(0, $this->file->append("nope"));
        $this->assertEquals(3, $this->temp->append("yes"));
    }

    function testAddForm() {
        $this->assertTrue( 
            $this->temp->addForm(
                array(
                    "foo" => "bar",
                    "more" => array(
                        "bah", "baz", "fuz"
                    ),
                ),
                array(
                    array(
                        "file" => __FILE__,
                        "name" => "upload",
                        "type" => "text/plain",
                    )
                )
            )
        );

        $file = str_replace("%", "%c", file_get_contents(__FILE__));
        $this->assertStringMatchesFormat(
            "--%x.%x\r\n".
            "Content-Disposition: form-data; name=\"foo\"\r\n".
            "\r\n".
            "bar\r\n".
            "--%x.%x\r\n".
            "Content-Disposition: form-data; name=\"more[0]\"\r\n".
            "\r\n".
            "bah\r\n".
            "--%x.%x\r\n".
            "Content-Disposition: form-data; name=\"more[1]\"\r\n".
            "\r\n".
            "baz\r\n".
            "--%x.%x\r\n".
            "Content-Disposition: form-data; name=\"more[2]\"\r\n".
            "\r\n".
            "fuz\r\n".
            "--%x.%x\r\n".
            "Content-Disposition: form-data; name=\"upload\"; filename=\"MessageBodyTest.php\"\r\n".
            "Content-Transfer-Encoding: binary\r\n".
            "Content-Type: text/plain\r\n".
            "\r\n".
            "{$file}\r\n".
            "--%x.%x--\r\n".
            "",
            str_replace("\r", "", $this->temp) // phpunit replaces \r\n with \n
        );
    }

    function testEtag() {
        $s = stat(__FILE__);
        $this->assertEquals(
            sprintf(
                "%lx-%lx-%lx", 
                $s["ino"],$s["mtime"],$s["size"]
            ),
            $this->file->etag()
        );
        $this->assertEquals(crc32(""), $this->temp->etag());
    }

    function testToStream() {
        $this->file->toStream($f = fopen("php://temp", "w")); 
        fseek($f, 0, SEEK_SET);
        $this->assertEquals(
            file_get_contents(__FILE__), 
            fread($f, filesize(__FILE__))
        );
    }

    function testToCallback() {
        $s = "";
        $this->file->toCallback(
            function($body, $string) use (&$s) { $s.=$string; }
        );
        $this->assertEquals($s, (string) $this->file);
    }

    function testClone() {
        $this->assertEquals((string) $this->file, (string) clone $this->file);
    }
}
