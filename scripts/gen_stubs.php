#!/usr/bin/env php
<?php

function m($m, $c = null) {
	$n = "";
	if ($c && $c->isInterface()) {
		$m ^= ReflectionMethod::IS_ABSTRACT;
	}
	foreach (Reflection::getModifierNames($m) as $mn) {
		$n .= $mn . " ";
	}
	return $n;
}
function t($p) {
	if ($c = $p->getClass()) return "\\" . $c->getName() . " ";
	if ($p->isArray()) return "array ";
}
function c($n, $c) {
    $_=$c;
    while ($c = $c->getParentClass()) {
        if (array_key_exists($n, $c->getConstants())) {
            return false;
        }
    }
    $c=$_;
    foreach ((array) $c->getInterfaces() as $i) {
        if (array_key_exists($n, $i->getConstants()) || !c($n, $i)) {
            return false;
        }
    }
    return true;
}

ob_start(function($s) {
    // redirect any output to stderr
    fwrite(STDERR, $s);
    return true;
});

$out = STDOUT;
switch ($argc) {
    default:
    case 3:
        $out = fopen($argv[2], "w") or die;
    case 2:
        $ext = $argv[1];
        break;
    
    case 1:
        die(sprintf($out, "Usage: %s <ext>\n", $argv[0]));
}

fprintf($out, "<?php\n\n");

$ext = new ReflectionExtension($ext);

$constants  = array();
$functions  = array();
$structures = array();

// split up by namespace first
foreach ($ext->getConstants() as $constant => $value) {
    $ns = ($nsend = strrpos($constant, "\\")) ? substr($constant, 0, $nsend++) : "";
    $cn = substr($constant, $nsend);
    $constants[$ns][$cn] = $value;
}
foreach ($ext->getFunctions() as $f) {
    /* @var $f ReflectionFunction */
    $ns = $f->inNamespace() ? $f->getNamespaceName() : "";
    $functions[$ns][$f->getShortName()] = $f;
}
foreach ($ext->getClasses() as $c) {
    /* @var $c ReflectionClass */
    $ns = $c->inNamespace() ? $c->getNamespaceName() : "";
    $structures[$ns][$c->getShortName()] = $c;
}

$namespaces = array_unique(array_merge(
        array_keys($constants),
        array_keys($functions),
        array_keys($structures)
));

// simple sort
natsort($namespaces);

foreach ($namespaces as $ns) {
    fprintf($out, "namespace %s%s\n{\n", $ns, strlen($ns) ? " " : "");
    //
    if (isset($constants[$ns])) {
        ksort($constants[$ns], SORT_NATURAL);
        foreach ($constants[$ns] as $cn => $value) {
            fprintf($out, "\tconst %s = %s;\n", $cn, var_export($value, true));
        }
    }
    //
    if (isset($functions[$ns])) {
        ksort($functions[$ns], SORT_NATURAL);
        foreach ($functions[$ns] as $fn => $f) {
            /* @var $f ReflectionFunction */
            fprintf($out, "\n\tfunction %s(", $fn);
            $ps = array();
            foreach ($f->getParameters() as $p) {
                $p1 = sprintf("%s%s\$%s", t($p), 
                        $p->isPassedByReference()?"&":"", trim($p->getName(), "\""));
                if ($p->isOptional()) {
                    if ($p->isDefaultValueAvailable()) {
                        $p1 .= sprintf(" = %s", 
                                var_export($p->getDefaultValue(), true));
                    } elseif (!($p->isArray() || $p->getClass()) || $p->allowsNull()) {
                        $p1 .= " = NULL";
                    } elseif ($p->isArray()) {
                        $p1 .= " = array()";
                    }
                }
                $ps[] = $p1;
            }
            fprintf($out, "%s) {\n\t}\n", implode(", ", $ps));
        }
    }
    //
    if (isset($structures[$ns])) {
        uasort($structures[$ns], function ($a, $b) {
            /* @var $a ReflectionClass */
            /* @var $b ReflectionClass */
            $score = array_sum([
                -!$a->isInterface()+
                -!$a->isAbstract()+
                -!$a->isTrait()+
                -!substr_compare($a->getShortName(), "Exception", -strlen("Exception")),
                +!$b->isInterface()+
                +!$b->isAbstract()+
                +!$b->isTrait()+
                -!substr_compare($b->getShortName(), "Exception", -strlen("Exception")),
            ]);
            
            if ($score) {
                return -$score;
            }
            return strnatcmp($a->getShortName(), $b->getShortName());
        });
        foreach ($structures[$ns] as $cn => $c) {
            fprintf($out, "\n\t%s%s %s ", m($c->getModifiers()), 
                    $c->isInterface() ? "interface":"class", $c->getShortName());
            if ($p = $c->getParentClass()) {
                fprintf($out, "extends \\%s ", $p->getName());
            }
            if ($i = $c->getInterfaceNames()) {
                fprintf($out, "implements \\%s ", 
                        implode(", \\", array_filter($i, function($v) {
                            return $v != "Traversable";
                            
                        }))
                );
            }
            fprintf($out, "\n\t{\n");

            $_=0;
            foreach ($c->getConstants() as $n => $v) {
                c($n, $c) and $_+=fprintf($out, "\t\tconst %s = %s;\n", $n, 
                        var_export($v, true));
            }
            $_ and fprintf($out, "\n");
            $_=0;
            foreach ($c->getProperties() as $p) {
                if ($p->getDeclaringClass()->getName() == $c->getName()) {
                    $_+=fprintf($out, "\t\t%s\$%s;\n", m($p->getModifiers()), 
                            $p->getName());
                }
            }
            $_ and fprintf($out, "\n");

            foreach ($c->getMethods() as $m) {
                if ($m->getDeclaringClass()->getName() == $c->getName()) {
                    fprintf($out, "\t\t%sfunction %s(", m($m->getModifiers(), $c), 
                            $m->getName());
                    $ps = array();
                    foreach ($m->getParameters() as $p) {
                        $p1 = sprintf("%s%s\$%s", t($p), 
                                $p->isPassedByReference()?"&":"", $p->getName());
                        if ($p->isOptional()) {
                            if ($p->isDefaultValueAvailable()) {
                                $p1 .= sprintf(" = %s", 
                                        var_export($p->getDefaultValue(), true));
                            } elseif (!($p->isArray() || $p->getClass()) || $p->allowsNull()) {
                                $p1 .= sprintf(" = NULL");
                            } elseif ($p->isArray()) {
                                $p1 .= " = array()";
                            }
                        }
                        $ps[] = $p1;
                    }
                    fprintf($out, "%s)", implode(", ", $ps));
                    if ($m->isAbstract()) {
                        fprintf($out, ";\n\n");
                    } else {
                        fprintf($out, " {\n\t\t}\n\n");
                    }
                }
            }

            fprintf($out, "\t}\n");
            
        }
    }
    //
    fprintf($out, "}\n\n");
}
