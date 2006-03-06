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
    protected $request;

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
     * Proxy to HttpRequest::setOptions()
     *
     * @param array $options
     * @return unknown
     */
    public function setOptions(array $options = null)
    {
        return $this->request->setOptions($options);
    }
    
    /**
     * Get associated HttpRequest instance
     *
     * @return HttpRequest
     */
    public function getRequest()
    {
        return $this->request;
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
        $this->request->setRawPostData(xmlrpc_encode_request($method, $params));
        $response = $this->request->send();
        if ($response->getResponseCode() != 200) {
            throw new Exception($response->getBody(), $response->getResponseCode());
        }
        return xmlrpc_decode($response->getBody(), 'utf-8');
    }
}

?>
