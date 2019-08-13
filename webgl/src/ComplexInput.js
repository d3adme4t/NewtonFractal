// This file is part of the NewtonFractal project.
// Copyright (C) 2019 Christian Bauer and Timon Foehl
// License: GNU General Public License version 3 or later,
// see the file LICENSE in the main directory.

import React from "react";
import Complex from "complex.js"
import { InputGroup, Intent } from "@blueprintjs/core";

class ComplexInput extends React.Component {
	constructor(props) {
		super(props);
		this.state = {
			intent: Intent.NONE,
			complex: ""
		}
	}

	inputChanged(event) {
		// Validate input
		let valid = true;
		let input = event.target.value;
		try {
			var c = new Complex(input);
			valid = !c.isNaN();
		} catch (e) {
			valid = false;
		}

		// Set input field
		if (valid) {
			this.setState({
				intent: Intent.NONE,
				complex: event.target.value
			});
		} else {
			this.setState({
				intent: Intent.DANGER,
				complex: event.target.value
			});
		}
	}

	render() {
		return (
			<InputGroup
				disabled={this.props.disabled} fill={this.props.fill} 
				large={this.props.large} leftIcon={this.props.leftIcon} intent={this.state.intent}
				placeholder={this.props.placeholder} rightElement={this.props.rightElement}
				round={this.props.round} type={this.props.type} value={this.state.complex}
				onChange={(event) => this.inputChanged(event)}>
			</InputGroup>
		);
	}
}

export default ComplexInput;