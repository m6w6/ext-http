<?php

/**
 * XMLRPC Client, very KISS
 * $Id$
 * 
 * NOTE: requires ext/xmlrpc
 * 
 * Usage:
 * <code>
 * <?php
 * $rpc = new XmlRpcClient('http://mike:secret@example.com/cgi-bin/vpop-xmlrpc', 'vpop');
 * $rpc->options = array(array('compress' => true));
 * try {
 *     print_r($rpc->listdomain(array('domain' => 'example.com')));
 * } catch (Exception $ex) {
 *     echo $ex;
 * }
 * ?>
 * </code>
 * 
 * @copyright   Michael Wallner, <mike@iworks.at>
 * @license     BSD, revised
 * @package     pecl/http
 * @version     $Revision$
 */
class XmlRpcClient
{
    /**
     * RPC namespace
     *
     * @var string
     */
    public $namespace;
    
    /**
     * HttpRequest instance
     *
     * @var HttpRequest
     */
    public $request;

    /**
     * Constructor
     *
     * @param string $url RPC endpoint
     * @param string $namespace RPC namespace
     */
    public function __construct($url, $namespace = '')
    {
        $this->namespace = $namespace;
        $this->request = new HttpRequest($url, HTTP_METH_POST);
        $this->request->setContentType('text/xml');
    }

    /**
     * RPC method proxy
     *
     * @param string $method RPC method name
     * @param array $params RPC method arguments
     * @return mixed decoded RPC response
     * @throws Exception
     */
    public function __call($method, array $params)
    {
        if ($this->namespace) {
            $method = $this->namespace .'.'. $method;
        }
        
        $data = xmlrpc_encode_request($method, $params);
        $this->request->setRawPostData($data);
        
        $response = $this->request->send();
        if ($response->getResponseCode() != 200) {
            throw new Exception(
                $response->getResponseStatus(), 
                $response->getResponseCode()
            );
        }
        $data = xmlrpc_decode($response->getBody(), 'utf-8');
        
        if (isset($data['faultCode'], $data['faultString'])) {
            throw new Exception(
                $data['faultString'],
                $data['faultCode']
            );
        }
        
        return $data;
    }
    
    public function __set($what, $params)
    {
        return call_user_func_array(
            array($this->request, "set$what"),
            $params
        );
    }
    
    public function __get($what)
    {
        return call_user_func(
            array($this->request, "get$what")
        );
    }
}

?>
