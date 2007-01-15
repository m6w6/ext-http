<?php

XmlRpcServer::setContentType("text/xml");
XmlRpcServer::capture();

/**
 * XMLRPC Server, very KISS
 * $Id$
 * 
 * NOTE: requires ext/xmlrpc
 * 
 * Usage:
 * <code>
 * <?php
 * 	class Handler extends XmlRpcRequestHandlerStub {
 * 		public function xmlrpcPing(array $values) {
 * 			return true;
 * 		}
 * 	}
 * 	try {
 * 		XmlRpcServer::factory("namespace")->registerHandler(new Handler);
 * 		XmlRpcServer::run();
 * 	} catch (Exception $ex) {
 * 		XmlRpcServer::error($ex->getCode(), $ex->getMessage());
 *  }
 * </code>
 * 
 * @copyright   Michael Wallner, <mike@iworks.at>
 * @license     BSD, revised
 * @package     pecl/http
 * @version     $Revision$
 */

class XmlRpcServer extends HttpResponse
{
	/**
	 * Server charset
	 *
	 * @var string
	 */
	public static $encoding = "iso-8859-1";
	
	/**
	 * RPC namespace
	 *
	 * @var string
	 */
	public $namespace;
	
	/**
	 * RPC handler attached to this server instance
	 *
	 * @var XmlRpcRequestHandler
	 */
	protected $handler;
	
	private static $xmlreq;
	private static $xmlrpc;
	private static $refcnt = 0;
	private static $handle = array();
	
	/**
	 * Create a new XmlRpcServer instance
	 *
	 * @param string $namespace
	 * @param string $encoding
	 */
	public function __construct($namespace)
	{
		$this->namespace = $namespace;
		self::initialize();
	}
	
	/**
	 * Destructor
	 */
	public function __destruct()
	{
		if (self::$refcnt && !--self::$refcnt) {
			xmlrpc_server_destroy(self::$xmlrpc);
		}
	}
	
	/**
	 * Static factory
	 *
	 * @param string $namespace
	 * @return XmlRpcServer
	 */
	public static function factory($namespace)
	{
		return new XmlRpcServer($namespace);
	}
	
	/**
	 * Run all servers and send response
	 *
	 * @param array $options
	 */
	public static function run(array $options = null)
	{
		self::initialize(false, true);
		self::setContentType("text/xml; charset=". self::$encoding);
		echo xmlrpc_server_call_method(self::$xmlrpc, self::$xmlreq, null, 
			array("encoding" => self::$encoding) + (array) $options);
	}
	
	/**
	 * Test hook; call instead of XmlRpcServer::run()
	 *
	 * @param string $method
	 * @param array $params
	 * @param array $request_options
	 * @param array $response_options
	 */
	public static function test($method, array $params, array $request_options = null, array $response_options = null)
	{
		self::$xmlreq = xmlrpc_encode_request($method, $params, $request_options);
		self::run($response_options);
	}
	
	/**
	 * Optional XMLRPC error handler
	 *
	 * @param int $code
	 * @param string $msg
	 */
	public static function error($code, $msg, array $options = null)
	{
		echo xmlrpc_encode(array("faultCode" => $code, "faultString" => $msg),
			array("encoding" => self::$encoding) + (array) $options);
	}
	
	/**
	 * Register a single method
	 *
	 * @param string $name
	 * @param mixed $callback
	 * @param mixed $dispatch
	 * @param array $spec
	 */
	public function registerMethod($name, $callback, $dispatch = null, array $spec = null)
	{
		if (!is_callable($callback, false, $cb_name)) {
			throw new Exception("$cb_name is not a valid callback");
		}
		if (isset($dispatch)) {
			if (!is_callable($dispatch, false, $cb_name)) {
				throw new Exception("$cb_name is not a valid callback");
			}
			xmlrpc_server_register_method(self::$xmlrpc, $name, $dispatch);
			self::$handle[$name] = $callback;
		} else {
			xmlrpc_server_register_method(self::$xmlrpc, $name, $callback);
		}
		
		if (isset($spec)) {
			xmlrpc_server_add_introspection_data(self::$xmlrpc, $spec);
		}
	}
	
	/**
	 * Register an XmlRpcRequestHandler for this server instance
	 *
	 * @param XmlRpcRequestHandler $handler
	 */
	public function registerHandler(XmlRpcRequestHandler $handler)
	{
		$this->handler = $handler;
		
		foreach (get_class_methods($handler) as $method) {
			if (!strncmp($method, "xmlrpc", 6)) {
				$this->registerMethod(
					$this->method($method, $handler->getNamespace()), 
					array($handler, $method), array($this, "dispatch"));
			}
		}
		
		$handler->getIntrospectionData($spec);
		if (is_array($spec)) {
			xmlrpc_server_add_introspection_data(self::$xmlrpc, $spec);
		}
	}
	
	private function method($method, $namespace = null)
	{
		if (!strlen($namespace)) {
			$namespace = strlen($this->namespace) ? $this->namespace : "xmlrpc";
		}
		return $namespace .".". strtolower($method[6]) . substr($method, 7);
	}
	
	private function dispatch($method, array $params = null)
	{
		if (array_key_exists($method, self::$handle)) {
			return call_user_func(self::$handle[$method], $params);
		}
		throw new Exception("Unknown XMLRPC method: $method");
	}
	
	private static function initialize($server = true, $data = false)
	{
		if ($data) {
			if (!self::$xmlreq && !(self::$xmlreq = http_get_request_body())) {
				throw new Exception("Failed to fetch XMLRPC request body");
			}
		}
		if ($server) {
			if (!self::$xmlrpc && !(self::$xmlrpc = xmlrpc_server_create())) {
				throw new Exception("Failed to initialize XMLRPC server");
			}
			++self::$refcnt;
		}
	}
}

/**
 * XmlRpcRequestHandler
 * 
 * Define XMLRPC methods with an "xmlrpc" prefix, eg:
 * <code>
 * 	class IntOp implements XmlRpcRequestHandler {
 * 		public function getNamespace() {
 * 			return "int";
 * 		}
 * 		public function getInstrospectionData(array &$spec = null) {
 * 		}
 * 		// XMLRPC method name: int.sumValues
 *		public function xmlrpcSumValues(array $values) {
 *			 return array_sum($values);
 *		}
 * 	}
 * </code>
 */
interface XmlRpcRequestHandler
{
	public function getNamespace();
	public function getIntrospectionData(array &$spec = null);
}

/**
 * XmlRpcRequestHandlerStub
 */
abstract class XmlRpcRequestHandlerStub implements XmlRpcRequestHandler
{
	public function getNamespace()
	{
	}
	public function getIntrospectionData(array &$spec = null)
	{
	}
}

?>
