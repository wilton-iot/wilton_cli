
define([{{deps_line}}], function({{args_line}}) {
    "use strict";
    return {
        main: function() {
            var RESULT = {{code}};
            print(RESULT);
        }
    };
});
