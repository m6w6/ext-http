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
 * $rpc = new XmlRpcClient('http://mike:secret@example.com/cgi-bin/vpop-xmlrpc');
 * $rpc->__request->setOptions(array('compress' => true));
 * try {
 *     print_r($rpc->vpop->listdomain(array('domain' => 'example.com')));
 * } catch (Exception $ex) {
 *     echo $ex;
 * }
 * ?>
 * </code>
 * 
 * @copyright   Michael Wallner, <mike@iworks.at>
 * @license     BSD, revised
 * @package     pecl/http
 * @version	    $Revision$
 */
class XmlRpcClient
{
	/**
	 * RPC namespace
	 *
	 * @var string
	 */
	public $__namespace;
	
	/**
	 * HttpRequest instance
	 *
	 * @var HttpRequest
	 */
	public $__request;
	
	/**
	 * Client charset
	 * 
	 * @var string
	 */
	public $__encoding = "iso-8859-1";
	
	/**
	 * RPC options
	 * 
	 * @var array
	 */
	public $__options;
	
	/**
	 * Constructor
	 *
	 * @param string $url RPC endpoint
	 * @param string $namespace RPC namespace
	 * @param array  $options HttpRequest options
	 */
	public function __construct($url, $namespace = '', array $options = null)
	{
		$this->__request = new HttpRequest($url, HttpRequest::METH_POST, (array) $options);
		$this->__namespace = $namespace;
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
		if (strlen($this->__namespace)) {
			$method = $this->__namespace .'.'. $method;
		}
		$this->__request->setContentType("text/xml");
		$this->__request->setRawPostData(
			xmlrpc_encode_request($method, $params, 
				array("encoding" => $this->__encoding) + (array) $this->__options));
		$response = $this->__request->send();
		if ($response->getResponseCode() != 200) {
			throw new Exception(
				$response->getResponseStatus(), 
				$response->getResponseCode()
			);
		}
		
		$data = xmlrpc_decode($response->getBody(), $this->__encoding);
		if (xmlrpc_is_fault($data)) {
			throw new Exception(
				(string) $data['faultString'],
				(int) $data['faultCode']
			);
		}
		
		return $data;
	}
	
	/**
	 * Returns self, where namespace is set to variable name
	 *
	 * @param string $ns
	 * @return XmlRpcRequest
	 */
	public function __get($ns)
	{
		$this->__namespace = $ns;
		return $this;
	}
}

?>
