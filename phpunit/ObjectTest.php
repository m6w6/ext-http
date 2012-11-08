<?php

class eh extends http\Object {
}

class ObjectTest extends PHPUnit_Framework_TestCase {
    function testDefaultErrorHandling() {
        $this->assertEquals(http\Object::EH_NORMAL, http\Object::getDefaultErrorHandling());
        http\Object::setDefaultErrorHandling(http\Object::EH_SUPPRESS);
        $this->assertEquals(http\Object::EH_SUPPRESS, http\Object::getDefaultErrorHandling());
    }

    function testErrorHandling() {
        $eh = new eh;
        $this->assertEquals(eh::EH_NORMAL, $eh->getErrorHandling());
        $eh->setErrorHandling(eh::EH_SUPPRESS);
        $this->assertEquals(eh::EH_SUPPRESS, $eh->getErrorHandling());
    }

    function testSuppress() {
        http\Object::setDefaultErrorHandling(http\Object::EH_SUPPRESS);
        $o = new eh;
		$o->triggerError(E_USER_WARNING, http\Exception::E_UNKNOWN, "suppress");
    }

    function testException() {
        http\Object::setDefaultErrorHandling(http\Object::EH_THROW);
        $this->setExpectedException("http\\Exception");
        $o = new eh;
		$o->triggerError(E_USER_WARNING, http\Exception::E_UNKNOWN, "exception");
    }

    function testNormalError() {
        http\Object::setDefaultErrorHandling(http\Object::EH_NORMAL);
        $this->setExpectedException("PHPUnit_Framework_Error_Warning");
        $o = new eh;
		$o->triggerError(E_USER_WARNING, http\Exception::E_UNKNOWN, "warning");
    }

    function testSuppress2() {
        $eh = new eh;
        $eh->setErrorHandling(http\Object::EH_SUPPRESS);
        $eh->triggerError(E_USER_WARNING, http\Exception::E_UNKNOWN, "suppress");
    }

    function testException2() {
        $eh = new eh;
        $eh->setErrorHandling(http\Object::EH_THROW);
        $this->setExpectedException("http\\Exception");
        $eh->triggerError(E_USER_WARNING, http\Exception::E_UNKNOWN, "exception");
    }

    function testNormalError2() {
        $eh = new eh;
        $eh->setErrorHandling(http\Object::EH_NORMAL);
        $this->setExpectedException("PHPUnit_Framework_Error_Warning");
        $eh->triggerError(E_USER_WARNING, http\Exception::E_UNKNOWN, "warning");
    }

    function testUnknownDefaultErrorHandling() {
        $this->setExpectedException("PHPUnit_Framework_Error_Warning");
        http\Object::setDefaultErrorHandling(12345);
    }

    function testUnknownErrorHandling() {
        $eh = new eh;
        $this->setExpectedException("PHPUnit_Framework_Error_Warning");
        $eh->setErrorHandling(12345);
    }
}

