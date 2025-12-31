// Firefox Layout Measurement Tool
// Usage: Open testdocument.html in Firefox, open Console (F12), paste and run this script

(function() {
    const elements = ['html', 'body', 'h1', 'p', 'h3', 'form'];
    const results = [];

    console.log('=== Firefox Layout Measurements ===\n');

    elements.forEach(tagName => {
        const el = document.querySelector(tagName);
        if (!el) {
            console.log(`${tagName}: NOT FOUND`);
            return;
        }

        const computed = window.getComputedStyle(el);
        const rect = el.getBoundingClientRect();

        // Get all box model values
        const marginTop = parseFloat(computed.marginTop);
        const marginRight = parseFloat(computed.marginRight);
        const marginBottom = parseFloat(computed.marginBottom);
        const marginLeft = parseFloat(computed.marginLeft);

        const paddingTop = parseFloat(computed.paddingTop);
        const paddingRight = parseFloat(computed.paddingRight);
        const paddingBottom = parseFloat(computed.paddingBottom);
        const paddingLeft = parseFloat(computed.paddingLeft);

        const borderTop = parseFloat(computed.borderTopWidth);
        const borderRight = parseFloat(computed.borderRightWidth);
        const borderBottom = parseFloat(computed.borderBottomWidth);
        const borderLeft = parseFloat(computed.borderLeftWidth);

        const fontSize = parseFloat(computed.fontSize);
        const lineHeight = computed.lineHeight;

        // Calculate dimensions
        const borderBoxWidth = rect.width;
        const borderBoxHeight = rect.height;

        const contentWidth = borderBoxWidth - paddingLeft - paddingRight - borderLeft - borderRight;
        const contentHeight = borderBoxHeight - paddingTop - paddingBottom - borderTop - borderBottom;

        const info = {
            tag: tagName.toUpperCase(),
            borderBox: `${Math.round(borderBoxWidth)}x${Math.round(borderBoxHeight)}`,
            contentBox: `${Math.round(contentWidth)}x${Math.round(contentHeight)}`,
            margins: `${marginTop}px ${marginRight}px ${marginBottom}px ${marginLeft}px`,
            padding: `${paddingTop}px ${paddingRight}px ${paddingBottom}px ${paddingLeft}px`,
            border: `${borderTop}px ${borderRight}px ${borderBottom}px ${borderLeft}px`,
            fontSize: `${fontSize}px`,
            lineHeight: lineHeight,
            display: computed.display,
            position: `x=${Math.round(rect.left)} y=${Math.round(rect.top)}`
        };

        results.push(info);

        // Print detailed info
        console.log(`<${tagName}>:`);
        console.log(`  Border Box: ${info.borderBox}`);
        console.log(`  Content Box: ${info.contentBox}`);
        console.log(`  Margins (T R B L): ${info.margins}`);
        console.log(`  Padding (T R B L): ${info.padding}`);
        console.log(`  Border (T R B L): ${info.border}`);
        console.log(`  Font Size: ${info.fontSize}`);
        console.log(`  Line Height: ${info.lineHeight}`);
        console.log(`  Display: ${info.display}`);
        console.log(`  Position: ${info.position}`);

        // For elements with text content, measure first line
        if (el.textContent && el.textContent.trim()) {
            const range = document.createRange();
            const textNode = el.childNodes[0];
            if (textNode && textNode.nodeType === Node.TEXT_NODE) {
                range.selectNodeContents(textNode);
                const textRect = range.getBoundingClientRect();
                console.log(`  First text line: ${Math.round(textRect.width)}x${Math.round(textRect.height)}`);
            }
        }

        // For elements with children, show child count and types
        if (el.children.length > 0) {
            const childInfo = Array.from(el.children).map(c => c.tagName).join(', ');
            console.log(`  Children (${el.children.length}): ${childInfo}`);
        }

        console.log('');
    });

    // Generate C array format for easy copy-paste into test
    console.log('\n=== C Array Format ===');
    results.forEach(info => {
        const width = parseInt(info.borderBox.split('x')[0]);
        const height = parseInt(info.borderBox.split('x')[1]);
        console.log(`{"${info.tag.toLowerCase()}", ${width}, ${height}},`);
    });

    // Calculate what contributes to body height
    console.log('\n=== Body Height Breakdown ===');
    const body = document.querySelector('body');
    if (body) {
        const bodyComputed = window.getComputedStyle(body);
        const bodyRect = body.getBoundingClientRect();

        console.log(`Body border box height: ${Math.round(bodyRect.height)}px`);
        console.log(`  - Top padding: ${bodyComputed.paddingTop}`);
        console.log(`  - Top border: ${bodyComputed.borderTopWidth}`);
        console.log(`  - Bottom padding: ${bodyComputed.paddingBottom}`);
        console.log(`  - Bottom border: ${bodyComputed.borderBottomWidth}`);

        let childrenHeight = 0;
        let prevBottom = bodyRect.top + parseFloat(bodyComputed.paddingTop) + parseFloat(bodyComputed.borderTopWidth);

        console.log(`  - Children:`);
        Array.from(body.children).forEach((child, i) => {
            const childRect = child.getBoundingClientRect();
            const childComputed = window.getComputedStyle(child);
            const marginTop = parseFloat(childComputed.marginTop);
            const marginBottom = parseFloat(childComputed.marginBottom);

            const gap = childRect.top - prevBottom;
            console.log(`    [${i}] <${child.tagName.toLowerCase()}>: ${Math.round(childRect.height)}px (margins: ${marginTop}/${marginBottom}, gap: ${Math.round(gap)}px)`);

            childrenHeight += childRect.height + gap;
            prevBottom = childRect.bottom;
        });

        const bodyBottom = bodyRect.bottom - parseFloat(bodyComputed.paddingBottom) - parseFloat(bodyComputed.borderBottomWidth);
        const finalGap = bodyBottom - prevBottom;
        console.log(`  - Final gap to bottom: ${Math.round(finalGap)}px`);
        console.log(`  Total content height: ${Math.round(childrenHeight + finalGap)}px`);
    }

    // Show viewport info
    console.log('\n=== Viewport Info ===');
    console.log(`Window inner width: ${window.innerWidth}px`);
    console.log(`Window inner height: ${window.innerHeight}px`);
    console.log(`Document width: ${document.documentElement.scrollWidth}px`);
    console.log(`Document height: ${document.documentElement.scrollHeight}px`);

})();
