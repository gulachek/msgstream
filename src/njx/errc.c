/**
 * Copyright 2025 Nicholas Gulachek
 *
 * Use of this source code is governed by an MIT-style
 * license that can be found in the LICENSE file or at
 * https://opensource.org/licenses/MIT.
 */
#include "msgstream.h"

const char *msgstream_errname(int ec) {
	switch (ec) {
		{% for err in errorCodes %}
		case {{ err.name }}:
			return "{{ err.name }}";
			break;
		{% endfor %}
		default:
			return "(Unknown msgstream error code)";
	}
}

const char *msgstream_errstr(int ec) {
	switch (ec) {
		{% for err in errorCodes %}
		case {{ err.name }}:
			return "{{ err.msg }}";
			break;
		{% endfor %}
		default:
			return "(Unknown msgstream error code)";
	}
}
