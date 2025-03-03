/**
 * Copyright 2025 Nicholas Gulachek
 *
 * Use of this source code is governed by an MIT-style
 * license that can be found in the LICENSE file or at
 * https://opensource.org/licenses/MIT.
 */
#ifndef MSGSTREAM_ERRC_H
#define MSGSTREAM_ERRC_H

{% for errc in errorCodes %}
#define {{ errc.name }} {{ errc.value }}
{% endfor %}

#endif
