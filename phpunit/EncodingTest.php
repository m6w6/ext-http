<?php

class EncodingStreamTest extends PHPUnit_Framework_TestCase {
    function testChunkStatic() {
        $file = file(__FILE__);
        $cenc = array_reduce(
            $file,
            function($data, $line) {
                return $data . sprintf("%lx\r\n%s\r\n", strlen($line), $line);
            }
        ) . "0\r\n";

        $this->assertEquals(implode("", $file), http\Encoding\Stream\Dechunk::decode($cenc));
    }

    function testChunkNoteEncoded() {
        $s = "this is apparently not encodded\n";
        $this->assertEquals($s, @http\Encoding\Stream\Dechunk::decode($s));
    }

    function testChunkNotEncodedNotice() {
	    error_reporting(E_ALL);
        $this->setExpectedException("PHPUnit_Framework_Error_Notice", 
            "Data does not seem to be chunked encoded");
        $s = "this is apparently not encodded\n";
        $this->assertEquals($s, http\Encoding\Stream\Dechunk::decode($s));
    }

    function testChunkNotEncodedFail() {
        $s = "3\nis \nbetter than\n1\n";
        $this->assertNotEquals($s, @http\Encoding\Stream\Dechunk::decode($s));
    }

    function testChunkNotEncodedWarning1() {
        $this->setExpectedException("PHPUnit_Framework_Error_Warning", 
            "Expected LF at pos 8 of 20 but got 0x74");
        $s = "3\nis \nbetter than\n1\n";
        http\Encoding\Stream\Dechunk::decode($s);
    }

    function testChunkNotEncodedWarning2() {
        $this->setExpectedException("PHPUnit_Framework_Error_Warning", 
            "Expected CRLF at pos 10 of 24 but got 0x74 0x74");
        $s = "3\r\nis \r\nbetter than\r\n1\r\n";
        http\Encoding\Stream\Dechunk::decode($s);
    }

    function testChunkNotEncodedWarning3() {
        $this->setExpectedException("PHPUnit_Framework_Error_Warning", 
            "Expected chunk size at pos 6 of 27 but got trash");
        $s = "3\nis \nreally better than\n1\n";
        http\Encoding\Stream\Dechunk::decode($s);
    }

    function testChunkFlush() {
        $dech = new http\Encoding\Stream\Dechunk(http\Encoding\Stream::FLUSH_FULL);
        $file = file(__FILE__);
        $data = "";
        foreach ($file as $i => $line) {
            $dech = clone $dech;
            if ($i % 2) {
                $data .= $dech->update(sprintf("%lx\r\n%s\r\n", strlen($line), $line));
            } else {
                $data .= $dech->update(sprintf("%lx\r\n", strlen($line)));
                $data .= $dech->update($line);
                $data .= $dech->update("\r\n");
            }
            $this->assertFalse($dech->done());
        }
        $data .= $dech->update("0\r\n");
        $this->assertTrue($dech->done());
        $data .= $dech->finish();
        $this->assertEquals(implode("", $file), $data);
    }

    function testZlibStatic() {
        $file = file_get_contents(__FILE__);
        $this->assertEquals($file, 
            http\Encoding\Stream\Inflate::decode(
                http\Encoding\Stream\Deflate::encode(
                    $file, http\Encoding\Stream\Deflate::TYPE_GZIP
                )
            )
        );
        $this->assertEquals($file, 
            http\Encoding\Stream\Inflate::decode(
                http\Encoding\Stream\Deflate::encode(
                    $file, http\Encoding\Stream\Deflate::TYPE_ZLIB
                )
            )
        );
        $this->assertEquals($file, 
            http\Encoding\Stream\Inflate::decode(
                http\Encoding\Stream\Deflate::encode(
                    $file, http\Encoding\Stream\Deflate::TYPE_RAW
                )
            )
        );
    }

    function testZlibAutoFlush() {
        $defl = new http\Encoding\Stream\Deflate(http\Encoding\Stream::FLUSH_FULL);
        $infl = new http\Encoding\Stream\Inflate;

        for ($f = fopen(__FILE__, "rb"); !feof($f); $data = fread($f, 0x100)) {
            $infl = clone $infl;
            $defl = clone $defl;
            if (isset($data)) {
                $this->assertEquals($data, $infl->update($defl->update($data)));
            }
        }

        echo $infl->update($defl->finish());
        echo $infl->finish();
    }

    function testZlibWithoutFlush() {
        $defl = new http\Encoding\Stream\Deflate;
        $infl = new http\Encoding\Stream\Inflate;
        $file = file(__FILE__);
        $data = "";
        foreach ($file as $line) {
            $infl = clone $infl;
            $defl = clone $defl;
            if (strlen($temp = $defl->update($line))) {
                foreach(str_split($temp) as $byte) {
                    $data .= $infl->update($byte);
                }
            }
        }
        if (strlen($temp = $defl->finish())) {
            $data .= $infl->update($temp);
        }
        $data .= $infl->finish();
        $this->assertEquals(implode("", $file), $data);
    }

    function testZlibWithExplicitFlush() {
        $defl = new http\Encoding\Stream\Deflate;
        $infl = new http\Encoding\Stream\Inflate;
        $file = file(__FILE__);
        $data = "";
        foreach ($file as $line) {
            if (strlen($temp = $defl->update($line))) {
                $data .= $infl->update($temp);
            }
            if (strlen($temp = $defl->flush())) {
                $data .= $infl->update($temp);
            }
            $this->assertTrue($defl->done());
        }
        if (strlen($temp = $defl->finish())) {
            $data .= $infl->update($temp);
        }
        $this->assertTrue($defl->done());
        $data .= $infl->finish();
        $this->assertTrue($infl->done());
        $this->assertEquals(implode("", $file), $data);
    }

    function testInflateError() {
        $this->setExpectedException("PHPUnit_Framework_Error_Warning",
            "Could not inflate data: data error");
        http\Encoding\Stream\Inflate::decode("if this goes through, something's pretty wrong");
    }
}
